/*
 * @Author: Amadeus
 * @Date: 2024-04-17 17:38:50
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-19 18:14:01
 * @FilePath: /Amadeus/src/app/setting_element.hh
 * @Description:
 */
#pragma once
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <nlohmann/json.hpp>
#include <seastar/core/sstring.hh>
#include <seastar/json/json_elements.hh>

namespace amadeus {

using namespace seastar;
namespace bpo = boost::program_options;
using bpo_map = boost::program_options::variables_map;
using bpo_var = boost::program_options::variable_value;
using bpo_desc = boost::program_options::options_description;

class element_base {
 public:
    element_base() = default;

    element_base(const sstring &name)
    : _name(name) {}

    virtual ~element_base() = default;

    virtual void from_boost(const bpo_var &v) = 0;
    virtual boost::shared_ptr<bpo::option_description> to_boost_option() = 0;

    virtual void from_seastar_json(seastar::json::json_base_element *e) = 0;
    virtual void to_seastar_json(seastar::json::json_base_element *e) = 0;

    virtual void from_nlohmann_json(const nlohmann::json &j) = 0;
    virtual void to_nlohmann_json(nlohmann::json &j) = 0;

    virtual bool equal(element_base *e) = 0;
    virtual void move_from(element_base *e) = 0;
    virtual void copy_from(element_base *e) = 0;

 public:
    sstring _name; // 名称
};

template <typename T>
class element : public element_base {
 protected:
    friend struct global_settings;
    T _default_value;              // 没有在命令行或配置文件中指定某个选项时使用的值
    T _implicit_value;             // 指定了该选项但没有给出具体值时使用的值
    T _value;                      // 指定了该选项且给出具体值时使用的值
    bool _Isimplicit = false;      // 是否使用隐式值
    seastar::sstring _description; // 选项描述

 public:
    element(const sstring &name, const T &value, const sstring &description)
    : element_base(name)
    , _default_value(value)
    , _implicit_value(value)
    , _Isimplicit(false)
    , _value(value)
    , _description(description) {}

    element(const sstring &name, const T &value, const T &implicit_value, const sstring &description)
    : element_base(name)
    , _default_value(value)
    , _implicit_value(implicit_value)
    , _Isimplicit(true)
    , _value(value)
    , _description(description) {}

    element(const element<T> &other)
    : element_base(other.name)
    , _default_value(other._default_value)
    , _implicit_value(other.implicit_value)
    , _Isimplicit(other._Isimplicit)
    , _value(other._value)
    , _description(other._description) {}

    element(element<T> &&other) noexcept
    : element_base(std::move(other._name))
    , _default_value(std::move(other._default_value))
    , _implicit_value(std::move(other._implicit_value))
    , _Isimplicit(std::move(other._Isimplicit))
    , _value(std::move(other._value))
    , _description(std::move(other._description)) {}

    element<T> &operator=(const element<T> &other) {
        if (this != &other) {
            _name = other._name;
            _default_value = other._default_value;
            _implicit_value = other._implicit_value;
            _Isimplicit = other._Isimplicit;
            _value = other._value;
            _description = other._description;
        }
        return *this;
    }

    element<T> &operator=(element<T> &&other) noexcept {
        if (this != &other) {
            _name = std::move(other._name);
            _default_value = std::move(other._default_value);
            _implicit_value = std::move(other._implicit_value);
            _Isimplicit = std::move(other._Isimplicit);
            _value = std::move(other._value);
            _description = std::move(other._description);
        }
        return *this;
    }

    // 从boost设置_value
    virtual void from_boost(const bpo_var &v) override {
        _value = v.as<T>();
    }

    // 创建boost_option
    virtual boost::shared_ptr<bpo::option_description> to_boost_option() override {
        auto v = bpo::value<T>()->default_value(_value);
        if (_Isimplicit) v = v->implicit_value(_implicit_value);

        return boost::make_shared<bpo::option_description>(_name.c_str(), v, _description.c_str());
    }

    // 从seastar_json设置_value
    virtual void from_seastar_json(seastar::json::json_base_element *e) override {
        auto ee = static_cast<seastar::json::json_element<T> *>(e);
        _value = (*ee)();
    }

    // 设置seastar_json的值
    virtual void to_seastar_json(seastar::json::json_base_element *e) override {
        auto ee = static_cast<seastar::json::json_element<T> *>(e);
        *ee = _value;
    }

    // 从nlohmann_json设置_value
    virtual void from_nlohmann_json(const nlohmann::json &j) override {
        if (j.is_null()) {
            _value = _default_value;
        } else {
            try {
                _value = j.get<T>();
            } catch (...) { // ignore
            }
        }
    }

    // 设置nlohmann_json的值
    virtual void to_nlohmann_json(nlohmann::json &j) override {
        j[_name] = _value;
    }

    virtual bool equal(element_base *e) override {
        if (auto *src = dynamic_cast<element<T> *>(e)) {
            return _value == src->_value;
        } else {
            return false;
        }
    }

    virtual void move_from(element_base *e) override {
        if (auto *other = dynamic_cast<element<T> *>(e)) {
            _name = std::move(other->_name);
            _default_value = std::move(other->_default_value);
            _implicit_value = std::move(other->_implicit_value);
            _Isimplicit = std::move(other->_Isimplicit);
            _value = std::move(other->_value);
            _description = std::move(other->_description);
        } else {
            throw std::runtime_error("Incompatible types for move");
        }
    }

    virtual void copy_from(element_base *e) override {
        if (const auto *other = dynamic_cast<const element<T> *>(e)) {
            _name = other->_name;
            _default_value = other->_default_value;
            _implicit_value = other->_implicit_value;
            _Isimplicit = other->_Isimplicit;
            _value = other->_value;
            _description = other->_description;
        } else {
            throw std::runtime_error("Incompatible types for copy");
        }
    }

    const T &operator()() const noexcept {
        return _value;
    }

    operator T() const noexcept {
        return _value;
    }

    bool operator==(const element<T> &v) const {
        return _name == v._name && _value == v._value && _description == v._description;
    }
};

} // namespace amadeus