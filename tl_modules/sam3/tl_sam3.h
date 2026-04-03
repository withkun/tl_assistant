#ifndef __INC_SAM3_H
#define __INC_SAM3_H

#include "base/model.h"

class Sam3 : public Model {
public:
    ImageEmbedding encode_image(const cv::Mat &image) override;
    GenerateResponse generate(GenerateRequest &request) override;

    int32_t TOKEN_W_ = 32;
    int32_t input_size_ = 1008;
    explicit Sam3() {
        name_ = "sam3:latest";
        blobs_ = {
            {
                "image_encoder", Blob(
                   "https://huggingface.co/wkentaro/sam3-onnx-models/resolve/main/sam3_image_encoder.onnx",
                   "sha256:3dd676271e9c459463f7026d8ab2c6318672cd89511f89c30b34cdf0a2e67a3f"
                )
            },
            {
                "language_encoder", Blob(
                    "https://huggingface.co/wkentaro/sam3-onnx-models/resolve/main/sam3_language_encoder.onnx",
                    "sha256:af80dffcd8fc369281b0422295244ba22f64e8424e9eefa111ac8966e9ea9ba6"
                )
            },
            {
                "decoder", Blob(
                    "https://huggingface.co/wkentaro/sam3-onnx-models/resolve/main/sam3_decoder.onnx",
                    "sha256:aca3109a1bf87d1589bf2a4a61c641fd97624152b21c6e9a5aa85735db398884"
                )
            },
        };
    }
};
#endif //__INC_SAM3_H