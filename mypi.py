""" effects
none, negative, solarize, sketch, denoise, 
emboss, oilpaint, hatch, gpen, pastel, 
watercolor, film, blur, saturation, colorswap,
washedout, posterise, colorpoint, colorbalance,
cartoon, deinterlace1, deinterlace2
"""

""" whitebalance
off, auto, sunlight, cloudy, shade, tungsten,
fluorescent, incandescent, flash, horizon
"""

"""exposure
off, auto, night, nightpreview, backlight,
spotlight, sports, snow, beach, verylong,
fixedfps, antishake, fireworks
"""

# camera = picamera.PiCamera()
# camera.resolution = (2592, 1944)
# camera.framerate = 15

import io
import os
import time
from subprocess import call
from threading import Condition

import boto3
import cv2
import numpy as np
import picamera
import requests
from botocore.client import Config
from PIL import Image, ImageDraw, ImageFont

ACCESS_KEY_ID = os.environ["ACCESS_KEY_ID"]
ACCESS_SECRET_KEY = os.environ["ACCESS_SECRET_KEY"]
BUCKET_NAME = os.environ["AWS_S3_BUCKET"]


class StreamingOutput:
    def __init__(self):
        self.frame = None
        self.buffer = io.BytesIO()
        self.condition = Condition()

    def write(self, buf):
        if buf.startswith(b"\xff\xd8"):
            self.buffer.truncate()
            with self.condition:
                self.frame = self.buffer.getvalue()
                self.condition.notify_all()
            self.buffer.seek(0)
        return self.buffer.write(buf)


def __decode_bbox(anchors, raw_outputs, variances=[0.1, 0.1, 0.2, 0.2]):
    anchor_centers_x = (anchors[:, :, 0:1] + anchors[:, :, 2:3]) / 2
    anchor_centers_y = (anchors[:, :, 1:2] + anchors[:, :, 3:]) / 2
    anchors_w = anchors[:, :, 2:3] - anchors[:, :, 0:1]
    anchors_h = anchors[:, :, 3:] - anchors[:, :, 1:2]
    raw_outputs_rescale = raw_outputs * np.array(variances)
    predict_center_x = raw_outputs_rescale[:, :, 0:1] * anchors_w + anchor_centers_x
    predict_center_y = raw_outputs_rescale[:, :, 1:2] * anchors_h + anchor_centers_y
    predict_w = np.exp(raw_outputs_rescale[:, :, 2:3]) * anchors_w
    predict_h = np.exp(raw_outputs_rescale[:, :, 3:]) * anchors_h
    predict_xmin = predict_center_x - predict_w / 2
    predict_ymin = predict_center_y - predict_h / 2
    predict_xmax = predict_center_x + predict_w / 2
    predict_ymax = predict_center_y + predict_h / 2
    predict_bbox = np.concatenate(
        [predict_xmin, predict_ymin, predict_xmax, predict_ymax], axis=-1
    )
    return predict_bbox


def __generate_anchors(feature_map_sizes, anchor_sizes, anchor_ratios, offset=0.5):
    anchor_bboxes = []
    for idx, feature_size in enumerate(feature_map_sizes):
        cx = (
            np.linspace(0, feature_size[0] - 1, feature_size[0]) + 0.5
        ) / feature_size[0]
        cy = (
            np.linspace(0, feature_size[1] - 1, feature_size[1]) + 0.5
        ) / feature_size[1]
        cx_grid, cy_grid = np.meshgrid(cx, cy)
        cx_grid_expend = np.expand_dims(cx_grid, axis=-1)
        cy_grid_expend = np.expand_dims(cy_grid, axis=-1)
        center = np.concatenate((cx_grid_expend, cy_grid_expend), axis=-1)

        num_anchors = len(anchor_sizes[idx]) + len(anchor_ratios[idx]) - 1
        center_tiled = np.tile(center, (1, 1, 2 * num_anchors))
        anchor_width_heights = []

        for scale in anchor_sizes[idx]:
            ratio = anchor_ratios[idx][0]  # select the first ratio
            width = scale * np.sqrt(ratio)
            height = scale / np.sqrt(ratio)
            anchor_width_heights.extend(
                [-width / 2.0, -height / 2.0, width / 2.0, height / 2.0]
            )

        # the first scale, with different aspect ratios (except the first one)
        for ratio in anchor_ratios[idx][1:]:
            s1 = anchor_sizes[idx][0]  # select the first scale
            width = s1 * np.sqrt(ratio)
            height = s1 / np.sqrt(ratio)
            anchor_width_heights.extend(
                [-width / 2.0, -height / 2.0, width / 2.0, height / 2.0]
            )

        bbox_coords = center_tiled + np.array(anchor_width_heights)
        bbox_coords_reshape = bbox_coords.reshape((-1, 4))
        anchor_bboxes.append(bbox_coords_reshape)
    anchor_bboxes = np.concatenate(anchor_bboxes, axis=0)
    return anchor_bboxes


def __single_class_non_max_suppression(
    bboxes, confidences, conf_thresh=0.2, iou_thresh=0.5, keep_top_k=-1
):
    if len(bboxes) == 0:
        return []

    conf_keep_idx = np.where(confidences > conf_thresh)[0]

    bboxes = bboxes[conf_keep_idx]
    confidences = confidences[conf_keep_idx]

    pick = []
    xmin = bboxes[:, 0]
    ymin = bboxes[:, 1]
    xmax = bboxes[:, 2]
    ymax = bboxes[:, 3]

    area = (xmax - xmin + 1e-3) * (ymax - ymin + 1e-3)
    idxs = np.argsort(confidences)

    while len(idxs) > 0:
        last = len(idxs) - 1
        i = idxs[last]
        pick.append(i)

        # keep top k
        if keep_top_k != -1:
            if len(pick) >= keep_top_k:
                break

        overlap_xmin = np.maximum(xmin[i], xmin[idxs[:last]])
        overlap_ymin = np.maximum(ymin[i], ymin[idxs[:last]])
        overlap_xmax = np.minimum(xmax[i], xmax[idxs[:last]])
        overlap_ymax = np.minimum(ymax[i], ymax[idxs[:last]])
        overlap_w = np.maximum(0, overlap_xmax - overlap_xmin)
        overlap_h = np.maximum(0, overlap_ymax - overlap_ymin)
        overlap_area = overlap_w * overlap_h
        overlap_ratio = overlap_area / (area[idxs[:last]] + area[i] - overlap_area)

        need_to_be_deleted_idx = np.concatenate(
            ([last], np.where(overlap_ratio > iou_thresh)[0])
        )
        idxs = np.delete(idxs, need_to_be_deleted_idx)

    # TODO if the number of final bboxes is less than keep_top_k, we need to pad it.
    return conf_keep_idx[pick]


feature_map_sizes = [[33, 33], [17, 17], [9, 9], [5, 5], [3, 3]]
anchor_sizes = [
    [0.04, 0.056],
    [0.08, 0.11],
    [0.16, 0.22],
    [0.32, 0.45],
    [0.64, 0.72],
]
anchor_ratios = [[1, 0.62, 0.42]] * 5
anchors = __generate_anchors(feature_map_sizes, anchor_sizes, anchor_ratios)
anchors_exp = np.expand_dims(anchors, axis=0)


class MyPi:
    # camera = picamera.PiCamera()

    __slots__ = ""
    korean_classification = {0: "마스크", 1: "마스크 X"}
    colors = ((0, 255, 0), (255, 0, 0))

    def take_picture(self):
        camera = picamera.PiCamera()
        camera.rotation = 180
        camera.resolution = (2592, 1944)
        camera.framerate = 15

        camera.start_preview()
        # camera.image_effect = 'colorswap'
        camera.annotate_text_size = 60
        camera.annotate_text = "System Programming"
        time.sleep(5)
        print("PY: Taking a picture now...")
        camera.capture("random.png")
        camera.stop_preview()
        camera.close()
        print("PY: Took a picture!")

        print("PY: Detecting face masks...")
        self.detect_human()  # OpenCV를 이용한 얼굴 마스크 감지
        print("PY: Detected face masks!")

    def upload_picture(self):
        print("PY: Uploading a picture...")

        with open("random.png", "rb") as file:
            s3 = boto3.resource(
                "s3",
                aws_access_key_id=ACCESS_KEY_ID,
                aws_secret_access_key=ACCESS_SECRET_KEY,
                config=Config(signature_version="s3v4"),
            )
            s3.Bucket(BUCKET_NAME).put_object(
                Key="random.png", Body=file, ContentType="image/jpg"
            )
        print("PY: Uploaded a picture!")

    def upload_video(self):
        """WIP 동영상 업로드"""
        print("PY: Uploading a video...")

        s3 = boto3.resource(
            "s3",
            aws_access_key_id=ACCESS_KEY_ID,
            aws_secret_access_key=ACCESS_SECRET_KEY,
            config=Config(signature_version="s3v4"),
        )
        s3.Bucket(BUCKET_NAME).put_object(
            Key="video.mp4", Body=file, ContentType="image/jpg"
        )
        print("PY: Uploaded a video!")

    def take_video(self):
        camera = picamera.PiCamera()
        camera.resolution = (1280, 720)
        camera.framerate = 30

        print("PY: Taking a video now...")
        camera.start_preview()
        camera.start_recording("video.h264")
        time.sleep(10)
        camera.stop_recording()
        camera.stop_preview()
        camera.close()
        self.convert("video.h264", "video.mp4")
        print("PY: Took a video: video.mp4")

    def do_live(self):
        print("PY: Doing a stream live")
        camera.resolution = (1280, 720)
        camera.framerate = 30
        camera.rotation = 180
        output = StreamingOutput()
        camera.start_recording(output, format="mjpeg")

        for i in output.buffer:
            print(i)

        print("PY: Sleep")
        time.sleep(10)

        camera.stop_recording()
        print("PY: End of streaming")

    def convert(self, h264, mp4):
        # video.h264 -> video.mp4
        command = "MP4Box -add " + h264 + " " + mp4
        call([command], shell=True)
        print(f"\r\n{h264} => Video converted to mp4! \r\n")

    def test(self):  # 멀티 쓰레딩 연습용
        print("hey1")
        time.sleep(5)
        print("hey2")
        self.take_picture()
        time.sleep(5)
        print("hey3")

    # ------------------ OpenCV 사람 Mask 감지
    def __put_korean(self, img, text, point, color):
        pilimg = Image.fromarray(img)
        draw = ImageDraw.Draw(pilimg)
        fontsize = int(min(img.shape[:2]) * 0.04)
        font = ImageFont.truetype("NanumGothic.ttf", fontsize, encoding="utf-8")
        y = point[1] - font.getsize(text)[1]
        if y <= font.getsize(text)[1]:
            y = point[1] + font.getsize(text)[1]
        draw.text((point[0], y), text, color, font=font)
        img = np.asarray(pilimg)
        return img

    def __get_output_names(self, net):
        # layersNames = net.getLayerNames()
        # return [layersNames[i[0] - 1] for i in net.getUnconnectedOutLayersNames()]
        return ["loc_branch_concat", "cls_branch_concat"]

    def __inference(
        self,
        net,
        image,
        conf_thresh=0.5,
        iou_thresh=0.4,
        target_shape=(160, 160),
        draw_result=True,
    ):
        height, width, _ = image.shape
        blob = cv2.dnn.blobFromImage(image, scalefactor=1 / 255.0, size=target_shape)
        net.setInput(blob)
        y_bboxes_output, y_cls_output = net.forward(self.__get_output_names(net))
        # batch dimension 제거 (inference 용)
        y_bboxes = __decode_bbox(anchors_exp, y_bboxes_output)[0]
        y_cls = y_cls_output[0]
        # 빠르게 하기 위해 NMS 싱글 클래스 (멀티 클래스 말고)
        bbox_max_scores = np.max(y_cls, axis=1)
        bbox_max_score_classes = np.argmax(y_cls, axis=1)

        keep_idxs = __single_class_non_max_suppression(
            y_bboxes, bbox_max_scores, conf_thresh=conf_thresh, iou_thresh=iou_thresh
        )
        # keep_idxs  = cv2.dnn.NMSBoxes(y_bboxes.tolist(), bbox_max_scores.tolist(), conf_thresh, iou_thresh)[:,0]
        tl = round(0.002 * (height + width) * 0.5) + 1  # line thickness
        for idx in keep_idxs:
            conf = float(bbox_max_scores[idx])
            class_id = bbox_max_score_classes[idx]
            bbox = y_bboxes[idx]
            xmin = max(0, int(bbox[0] * width))
            ymin = max(0, int(bbox[1] * height))
            xmax = min(int(bbox[2] * width), width)
            ymax = min(int(bbox[3] * height), height)
            if draw_result:
                cv2.rectangle(
                    image,
                    (xmin, ymin),
                    (xmax, ymax),
                    MyPi.colors[class_id],
                    thickness=tl,
                )

                image = self.__put_korean(
                    image,
                    MyPi.korean_classification[class_id],
                    (xmin, ymin),
                    MyPi.colors[class_id],
                )  # 한글 폰트 처리

        return image

    def detect_human(self, name="random.png"):
        Net = cv2.dnn.readNet(
            "face_mask_detection.caffemodel", "face_mask_detection.prototxt"
        )  # 약 8000장 정도 학습된 모델, OpenCV에서 사용하기 위해 수정
        img = cv2.imread(name)
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        result = self.__inference(Net, img, target_shape=(260, 260))
        cv2.imwrite(name, result[:, :, ::-1])


if __name__ == "__main__":
    mypi = MyPi()
    # mypi.do_live()
    # detect_human("face_test2.jpg")
    # mypi.test()
    # mypi.upload_picture()
    # take_picture()
    # take_video()
