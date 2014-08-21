#include <Python.h>

#include <iostream>
#include <opencv2/imgproc/imgproc.hpp>
#include <boost/python.hpp>

#include "conversion.h"
#include "brisk/brisk.h"

#include <sys/time.h>

namespace py = boost::python;

// TODO: fix feference counting and add documentation

static cv::Mat get_gray_img(PyObject *p_img);
static PyObject* keypoints_ctopy(std::vector<cv::KeyPoint> keypoints);

static cv::Mat get_gray_img(PyObject *p_img) {
    // get image and convert if necessary
    NDArrayConverter cvt;
    cv::Mat img_temp = cvt.toMat(p_img);
    cv::Mat img;
    if (img_temp.channels() == 1) {
        img = img_temp;
    } else {
        cv::cvtColor(img_temp, img, CV_BGR2GRAY);
    }

    return img;
}

static PyObject* keypoints_ctopy(std::vector<cv::KeyPoint> keypoints) {
    size_t num_keypoints = keypoints.size();
    PyObject* ret_keypoints = PyList_New(num_keypoints);
    // import cv2
    PyObject* cv2_mod = PyImport_ImportModule("cv2");

    for(size_t i = 0; i < num_keypoints; ++i) {
        // cv2_keypoint = cv2.KeyPoint()
        // TODO: PyInstance_New is maybe better
        // cv2_keypoint has maybe a memory leak
        PyObject* cv2_keypoint = PyObject_CallMethod(cv2_mod, "KeyPoint", "");

        // build values
        PyObject* cv2_keypoint_size = Py_BuildValue("f", keypoints[i].size);
        PyObject* cv2_keypoint_angle = Py_BuildValue("f", keypoints[i].angle);
        PyObject* cv2_keypoint_response = Py_BuildValue("f", keypoints[i].response);
        PyObject* cv2_keypoint_pt_x = Py_BuildValue("f", keypoints[i].pt.x);
        PyObject* cv2_keypoint_pt_y = Py_BuildValue("f", keypoints[i].pt.y);

        // pack into tuple
        PyObject* cv2_keypoint_pt = PyTuple_New(2);
        PyTuple_SetItem(cv2_keypoint_pt, 0, cv2_keypoint_pt_x);
        PyTuple_SetItem(cv2_keypoint_pt, 1, cv2_keypoint_pt_y);

        // set attributes
        PyObject_SetAttrString(cv2_keypoint, "size", cv2_keypoint_size);
        PyObject_SetAttrString(cv2_keypoint, "angle", cv2_keypoint_angle);
        PyObject_SetAttrString(cv2_keypoint, "response", cv2_keypoint_response);
        PyObject_SetAttrString(cv2_keypoint, "pt", cv2_keypoint_pt);

        // add keypoint to list
        PyList_SetItem(ret_keypoints, i, cv2_keypoint);

        Py_DECREF(cv2_keypoint_size);
        Py_DECREF(cv2_keypoint_angle);
        Py_DECREF(cv2_keypoint_response);
        Py_DECREF(cv2_keypoint_pt_x);
        Py_DECREF(cv2_keypoint_pt_y);
        Py_DECREF(cv2_keypoint_pt);
    }

    Py_DECREF(cv2_mod);

    return ret_keypoints;
}

PyObject* create() {
    brisk::BriskDescriptorExtractor* descriptor_extractor = new brisk::BriskDescriptorExtractor();
    return PyCObject_FromVoidPtr(static_cast<void*>(descriptor_extractor), NULL);
}

void destroy(PyObject* p_descriptor_extractor) {
    brisk::BriskDescriptorExtractor* descriptor_extractor =
            static_cast<brisk::BriskDescriptorExtractor*>(PyCObject_AsVoidPtr(p_descriptor_extractor));
    delete descriptor_extractor;
}

PyObject* detect(PyObject* p_descriptor_extractor, PyObject *p_img,
        PyObject *p_thresh, PyObject *p_octaves) {
    cv::Mat img = get_gray_img(p_img);

    // parse python arguments
    int thresh;
    int octaves;
    PyArg_Parse(p_thresh, "i", &thresh);
    PyArg_Parse(p_octaves, "i", &octaves);

    // detect keypoints
    cv::Ptr<cv::FeatureDetector> detector;
    detector = new brisk::BriskFeatureDetector(thresh, octaves);
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(img, keypoints);

    brisk::BriskDescriptorExtractor* descriptor_extractor =
            static_cast<brisk::BriskDescriptorExtractor*>(PyCObject_AsVoidPtr(p_descriptor_extractor));
    descriptor_extractor->computeAngles(img, keypoints);

    PyObject* ret_keypoints = keypoints_ctopy(keypoints);

    return ret_keypoints;
}

PyObject* compute(PyObject* p_descriptor_extractor,
        PyObject *p_img, PyObject *p_keypoints) {
    cv::Mat img = get_gray_img(p_img);
    Py_ssize_t num_keypoints = PyList_Size(p_keypoints);

    std::vector<cv::KeyPoint> keypoints;

    for(Py_ssize_t i = 0; i < num_keypoints; ++i) {
        keypoints.push_back(cv::KeyPoint());
        PyObject* cv2_keypoint = PyList_GetItem(p_keypoints, i);

        // get attributes
        PyObject* cv2_keypoint_size = PyObject_GetAttrString(cv2_keypoint, "size");
        PyObject* cv2_keypoint_angle = PyObject_GetAttrString(cv2_keypoint, "angle");
        PyObject* cv2_keypoint_response = PyObject_GetAttrString(cv2_keypoint, "response");
        PyObject* cv2_keypoint_pt = PyObject_GetAttrString(cv2_keypoint, "pt");
        PyObject* cv2_keypoint_pt_x = PyTuple_GetItem(cv2_keypoint_pt, 0);
        PyObject* cv2_keypoint_pt_y = PyTuple_GetItem(cv2_keypoint_pt, 1);

        // set data
        PyArg_Parse(cv2_keypoint_size, "f", &keypoints[i].size);
        PyArg_Parse(cv2_keypoint_angle, "f", &keypoints[i].angle);
        PyArg_Parse(cv2_keypoint_response, "f", &keypoints[i].response);
        PyArg_Parse(cv2_keypoint_pt_x, "f", &keypoints[i].pt.x);
        PyArg_Parse(cv2_keypoint_pt_y, "f", &keypoints[i].pt.y);

        Py_DECREF(cv2_keypoint_size);
        Py_DECREF(cv2_keypoint_angle);
        Py_DECREF(cv2_keypoint_response);
        Py_DECREF(cv2_keypoint_pt_x);
        Py_DECREF(cv2_keypoint_pt_y);
        Py_DECREF(cv2_keypoint_pt);
        // TODO: decrement reference doesn't work
        // Py_DECREF(cv2_keypoint);
    }

    cv::Mat descriptors;
    brisk::BriskDescriptorExtractor* descriptor_extractor =
            static_cast<brisk::BriskDescriptorExtractor*>(PyCObject_AsVoidPtr(p_descriptor_extractor));
    descriptor_extractor->compute(img, keypoints, descriptors);

    NDArrayConverter cvt;
    PyObject* ret = PyList_New(2);
    PyObject* ret_keypoints = keypoints_ctopy(keypoints);
    PyList_SetItem(ret, 0, ret_keypoints);
    PyList_SetItem(ret, 1, cvt.toNDArray(descriptors));
    // TODO: decrement reference doesn't work
    // Py_DECREF(ret_keypoints);

    return ret;
}

static void init() {
    Py_Initialize();
    import_array();
}

BOOST_PYTHON_MODULE(pybrisk) {
    init();
    py::def("create", create);
    py::def("destroy", destroy);
    py::def("detect", detect);
    py::def("compute", compute);
}
