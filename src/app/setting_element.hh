/*
 * @Author: Amadeus
 * @Date: 2024-04-17 17:38:50
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-19 11:45:37
 * @FilePath: /Amadeus/src/app/setting_element.hh
 * @Description:
 */
#pragma once
#include <nlohmann/json.hpp>
#include <seastar/core/sstring.hh>
#include <seastar/json/json_elements.hh>
namespace amadeus {
using namespace seastar;

class Ielement {
public:
    seastar::sstring _name;  // 名称
public:
    virtual void from_seastar_json(seastar::json::json_base_element *e) = 0;
    virtual void to_seastar_json(seastar::json::json_base_element *e) = 0;
    virtual void from_nlohmann_json(const nlohmann::json &j) = 0;
    virtual void to_nlohmann_json(nlohmann::json &j) = 0;
    sstring name() { return _name; }
    // virtual bool equal(Ielement *e) = 0;
    virtual void move_from(Ielement *e) = 0;
    virtual void copy_from(Ielement *e) = 0;
};

template <typename T>
class element : public Ielement {
public:
    element(const sstring &name, const T &value, const sstring &description)
        : _value(value), _default_value(value), _description(description) {
        _name = name;
    }
    element(const sstring &name, const T &value, const T &default_value,
            const sstring &description)
        : _value(value),
          _default_value(default_value),
          _description(description) {
        _name = name;
    }
    element(const element<T> &other)
        : _value(other._value),
          _default_value(other._default_value),
          _description(other._description),
          _Isdefault(other._Isdefault) {
        _name = other._name;
    }
    element(element<T> &&e) noexcept
        : _value(std::move(e._value)),
          _default_value(std::move(e._default_value)),
          _description(std::move(e._description)),
          _Isdefault(e._Isdefault) {
        _name = std::move(e._name);
    }
    element<T> &operator=(const element<T> &other) {
        if (this != &other) {
            _name = other._name;
            _value = other._value;
            _default_value = other._default_value;
            _description = other._description;
            _Isdefault = other._Isdefault;
        }
        return *this;
    }
    element<T> &operator=(element<T> &&other) noexcept {
        if (this != &other) {
            _name = std::move(other._name);
            _value = std::move(other._value);
            _default_value = std::move(other._default_value);
            _description = std::move(other._description);
            _Isdefault = other._Isdefault;
            other._Isdefault = false;  // 修改源对象状态，如果适用
        }
        return *this;
    }
    virtual void from_seastar_json(
        seastar::json::json_base_element *e) override {
        auto ee = static_cast<seastar::json::json_element<T> *>(e);
        _value = (*ee)();
    }
    virtual void to_seastar_json(seastar::json::json_base_element *e) override {
        auto ee = static_cast<seastar::json::json_element<T> *>(e);
        *ee = _value;
    }
    virtual void from_nlohmann_json(const nlohmann::json &j) override {
        if (j.is_null()) {
            _value = _default_value;
        } else {
            try {
                _value = j.get<T>();
            } catch (...) {  // ignore
            }
        }
    }
    virtual void to_nlohmann_json(nlohmann::json &j) override {
        j[_name] = _value;
    }

    virtual void move_from(Ielement *e) override {
        if (auto *src = dynamic_cast<element<T> *>(e)) {
            _name = std::move(src->_name);
            _value = std::move(src->_value);
            _default_value = std::move(src->_default_value);
            _description = std::move(src->_description);
            _Isdefault = src->_Isdefault;
        } else {
            throw std::runtime_error("Incompatible types for move");
        }
    }

    virtual void copy_from(Ielement *e) override {
        if (const auto *src = dynamic_cast<const element<T> *>(e)) {
            _name = src->_name;
            _value = src->_value;
            _default_value = src->_default_value;
            _description = src->_description;
            _Isdefault = src->_Isdefault;
        } else {
            throw std::runtime_error("Incompatible types for copy");
        }
    }

    const T &operator()() const noexcept { return _value; }

    operator T() const noexcept { return _value; }

    bool operator==(const element<T> &v) const {
        return _name == v._name && _value == v._value &&
               _description == v._description;
    }

protected:
    T _value;                       // 值
    T _default_value;               // 默认值
    bool _Isdefault = false;        // 是否使用隐含值
    seastar::sstring _description;  // 描述
};

}  // namespace amadeus