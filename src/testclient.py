#
# Written by kuwingto, 17 March 2020
#

import os
import math
import random
import numpy as np
import cv2
import importlib.util

import matplotlib.pyplot as plt
import matplotlib.image as mpimg
import sys
import time

import zmq
import imagepack_pb2
from time import time


if __name__ == '__main__' :

    ##### connect to server  #####
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.connect("tcp://localhost:5555")
    
    ##### send message #####
    socket.send_string("imageRequest")
    
    ##### receive message ####
    message = socket.recv()
    print("message recv")
    
    ##### parse message ####
    message_input = imagepack_pb2.imagepack()
    message_input.ParseFromString(message)
    
    for i in range(len(message_input.imgs) ):
        print("received image width = ", message_input.imgs[i].width)
        print("received image height = ", message_input.imgs[i].height)

        nparr = np.frombuffer(message_input.imgs[i].image_data,dtype=np.ubyte)
        print("data shape = ",nparr.shape)
        img = nparr.reshape(message_input.imgs[i].height,message_input.imgs[i].width)
        print("image shape = ",img.shape)
        
        stacked_img = np.stack((img,)*3, axis=-1) ##rearrange rgb
        imgplot = plt.imshow(stacked_img)
        plt.show()
