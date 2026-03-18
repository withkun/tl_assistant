#include "ort_utils.h"

size_t GetOrtValueByteSize(const Ort::Value &value) {
    if (value.IsTensor()) {
        const auto type_info = value.GetTensorTypeAndShapeInfo();
        switch (type_info.GetElementType()) {
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
                return type_info.GetElementCount() * sizeof(float);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
                return type_info.GetElementCount() * sizeof(double);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
                return type_info.GetElementCount() * sizeof(int8_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
                return type_info.GetElementCount() * sizeof(int16_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
                return type_info.GetElementCount() * sizeof(int32_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
                return type_info.GetElementCount() * sizeof(int64_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
                return type_info.GetElementCount() * sizeof(uint8_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
                return type_info.GetElementCount() * sizeof(uint16_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
                return type_info.GetElementCount() * sizeof(uint32_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
                return type_info.GetElementCount() * sizeof(uint64_t);
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
                return type_info.GetElementCount() * sizeof(bool);
            default:
                throw std::runtime_error("Unsupported tensor element type");
        }
    } else if (value.IsSparseTensor()) {
    //    // 对于序列，需要计算所有元素的总字节大小
    //    size_t total_size = 0;
    //    size_t element_count = value.GetValueCount();
    //    for (size_t i = 0; i < element_count; ++i) {
    //        Ort::Value element = value.GetValue(i, nullptr);
    //        total_size += GetOrtValueByteSize(element);
    //    }
    //
    //    return total_size;
    //} else if (value.IsMap()) {
    //    // 对于映射，需要分别计算键和值的字节大小
    //    // 注意：这里简化处理，实际可能需要更复杂的逻辑
    //    return 0; // 映射类型处理较为复杂，需要根据具体键值类型实现
    }
    // 其他类型返回0
    return 0;
}