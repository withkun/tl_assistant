//
// Created by njtl007 on 2024/11/28.
//

#ifndef TL_ASSISTANT_TL_WIDGETS_SHAPE_TO_JSON_H_
#define TL_ASSISTANT_TL_WIDGETS_SHAPE_TO_JSON_H_

#include "tl_shape.h"

#include <nlohmann/json.hpp>

// 自定义型nlohmann是不知道如何进行序列化, 需要实现to_json/from_json提供序列化操作.
namespace nlohmann {
void to_json(nlohmann::json &json, const QString &value);
void from_json(const nlohmann::json &json, QString &value);

void to_json(nlohmann::json &json, const TlShape &shape);
void from_json(const nlohmann::json &json, TlShape &shape);

void to_json(nlohmann::ordered_json &json, const QString &value);
void from_json(const nlohmann::ordered_json &json, QString &value);

void to_json(nlohmann::ordered_json &json, const TlShape &shape);
void from_json(const nlohmann::ordered_json &json, TlShape &shape);
} // namespace nlohmann

#endif// TL_ASSISTANT_TL_WIDGETS_SHAPE_TO_JSON_H_
