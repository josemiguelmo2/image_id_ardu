/* Edge Impulse ingestion SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* Includes ---------------------------------------------------------------- */
#include <face_recognition_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <SPI.h>
#include <SD.h>
#include <stdint.h>
#include <stdlib.h>

#define DWORD_ALIGN_PTR(a)   ((a & 0x3) ?(((uintptr_t)a + 0x4) & ~(uintptr_t)0x3) : a)
/*
 ** NOTE: If you run into TFLite arena allocation issue.
 **
 ** This may be due to may dynamic memory fragmentation.
 ** Try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt (create
 ** if it doesn't exist) and copy this file to
 ** `<ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/`.
 **
 ** See
 ** (https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the-installed-cores-located-)
 ** to find where Arduino installs cores on your machine.
 **
 ** If the problem persists then there's not enough memory for this model and application.
 */

/* Edge Impulse ------------------------------------------------------------- */

/**
 * @brief      Check if new serial data is available
 *
 * @return     Returns number of available bytes
 */
int ei_get_serial_available(void)
{
    return Serial.available();
}

/**
 * @brief      Get next available byte
 *
 * @return     byte
 */
char ei_get_serial_byte(void)
{
    return Serial.read();
}

/* Private variables ------------------------------------------------------- */
static bool is_initialised = false;

/*
** @brief points to the output of the capture
*/

static uint8_t *snapshot_buf;
// uint8_t *frame_buffer = NULL;
#define RGB_CHANNELS 3
const char *INFO_FILE = "/res_data.txt";
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    // comment out the below line to cancel the wait for USB connection (needed for native USB)
    while (!Serial)
        ;
    Serial.println("Edge Impulse Inferencing Demo");

    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tImage resolution: %dx%d\n", EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

    if (!SD.begin(4))
    {
        Serial.println("Initialization failed!\n");
        while (1)
            ;
    }
    Serial.println("Initialization done\n");
}

/**
 * @brief      Get data and run inferencing
 *
 * @param[in]  debug  Get debug info if true
 */

void loop()
{
    bool stop_inferencing = false;

    while (stop_inferencing == false)
    {
        ei_printf("\nStarting inferencing in 2 seconds...\n");
        // Test if parsing succeeds.

        ei_printf("Picking photo...\n");
        File resolutions = SD.open(INFO_FILE);
        if (!resolutions)
        {
            Serial.println("Error al abrir el archivo resolutions");
            while (1)
                ;
        }
        String res_data = "";
        while (resolutions.available())
        {
            res_data = resolutions.readString();
        }
        Serial.println(res_data);
        resolutions.close();

        int startIndex = 0;
        int endIndex = 0;

        while (endIndex >= 0)
        {
            endIndex = res_data.indexOf('\n', startIndex);
            if (endIndex >= 0)
            {
                String linea = res_data.substring(startIndex, endIndex);
                int commaIndex1 = linea.indexOf(',');
                int commaIndex2 = linea.indexOf(',', commaIndex1 + 1);
                if (commaIndex1 >= 0 && commaIndex2 >= 0)
                {
                    String nombre = linea.substring(0, commaIndex1);
                    int width = linea.substring(commaIndex1 + 1, commaIndex2).toInt();
                    int height = linea.substring(commaIndex2 + 1).toInt();

                    // // Hacer algo con los valores leídos
                    Serial.print("Nombre: ");
                    Serial.println(nombre);
                    Serial.print("Width: ");
                    Serial.println(width);
                    Serial.print("Height: ");
                    Serial.println(height);
                    File image = SD.open("/" + nombre + ".bmp", FILE_READ);
                    if (!image)
                    {
                        Serial.println("Error al abrir el archivo images");
                        while (1)
                            ;
                    }
                    int fileSize = image.size();
                    void *temp_mem = NULL;
                    uint8_t *temp_buf = NULL;
                    temp_mem = ei_malloc(width * height * RGB_CHANNELS);
                    //  void *snapshot_mem = ei_malloc(EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * RGB_CHANNELS);
                    temp_buf = (uint8_t *)DWORD_ALIGN_PTR((uintptr_t)temp_mem);
                    while (image.available())
                    {
                        image.read(temp_buf, fileSize);
                    }
                    image.close();
                    // if (width != EI_CLASSIFIER_INPUT_WIDTH && height != EI_CLASSIFIER_INPUT_HEIGHT)
                    //     ei::image::processing::crop_and_interpolate_rgb888(temp_buf, width, height, temp_buf, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);

                    startIndex = endIndex + 1;

                    snapshot_buf = temp_buf;
                    ei::signal_t signal;
                    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
                    signal.get_data = &ei_camera_get_data;

                    // run the impulse: DSP, neural network and the Anomaly algorithm
                    ei_impulse_result_t result = {0};

                    EI_IMPULSE_ERROR ei_error = run_classifier(&signal, &result, debug_nn);
                    if (ei_error != EI_IMPULSE_OK)
                    {
                        ei_printf("Failed to run impulse (%d)\n", ei_error);
                        //  ei_free(snapshot_mem);
                        break;
                    }
                    // print the predictions
                    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                              result.timing.dsp, result.timing.classification, result.timing.anomaly);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
                    bool bb_found = result.bounding_boxes[0].value > 0;
                    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++)
                    {
                        auto bb = result.bounding_boxes[ix];
                        if (bb.value == 0)
                        {
                            continue;
                        }

                        ei_printf("    %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\n", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
                    }

                    if (!bb_found)
                    {
                        ei_printf("    No objects found\n");
                    }
#else
                    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
                    {
                        ei_printf("    %s: %.5f\n", result.classification[ix].label,
                                  result.classification[ix].value);
                    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
                    ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif
#endif

                    while (ei_get_serial_available() > 0)
                    {
                        if (ei_get_serial_byte() == 'b')
                        {
                            ei_printf("Inferencing stopped by user\r\n");
                            stop_inferencing = true;
                        }
                    }
                    if (temp_mem)
                    {
                        ei_free(snapshot_buf);
                        ei_free(temp_buf);
                        ei_free(temp_mem);
                    }
                }
            }
        }
    }
}
       void calculate_crop_dims(
                int srcWidth,
                int srcHeight,
                int dstWidth,
                int dstHeight,
                int &cropWidth,
                int &cropHeight)
            {
                // first, trim the largest axis to match destination aspect ratio
                // calculate by fixing the smaller axis
                if (srcWidth > srcHeight)
                {
                    cropWidth = (uint32_t)(dstWidth * srcHeight) / dstHeight; // cast in case int is small
                    cropHeight = srcHeight;
                }
                else
                {
                    cropHeight = (uint32_t)(dstHeight * srcWidth) / dstWidth;
                    cropWidth = srcWidth;
                }
            }
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0)
    {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix + 2];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    // and done!
    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif

/* OV7675::readBuf() */
