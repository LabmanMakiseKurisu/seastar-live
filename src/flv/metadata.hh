/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   rtmp://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2023 bilibili
 */

#pragma once

#include <aom-av1.h>
#include <flv-header.h>
#include <mpeg4-aac.h>
#include <mpeg4-avc.h>
#include <mpeg4-hevc.h>

#include "frame/metadata_base.hh"

namespace amadeus {
namespace flv {

using namespace seastar;

class media_t;

// audio_meta_t和video_meta_t的基类
class meta_t : public meta_base {
 public:
    sstring codec; // 编码方式
    sstring codec_name() const;

    meta_t() = default;
    meta_t(int32_t _codecid, double _bitrate, double _datarate);
    virtual ~meta_t() = default;

    virtual sstring to_string() const override;

    friend std::ostream &operator<<(std::ostream &os, const meta_t *v) {
        return os << v->to_string();
    }

    friend std::ostream &operator<<(std::ostream &os, const meta_t &v) {
        return os << &v;
    }
};

// flv 音频元数据
struct audio_meta_t : public meta_t {
 public:
    uint32_t samplerate = 0;   // 采样频率（HZ）
    uint32_t samplesize = 0;   // 采样位深
    uint8_t channel_count = 0; // 声道数
    bool stereo = false;       // 是否为双声道

    struct mpeg4_aac_t aac; // aac 元数据

 public:
    audio_meta_t();
    audio_meta_t(const audio_meta_t &x);
    // audio_meta_t(audio_meta_t &&x);
    virtual ~audio_meta_t() = default;

    audio_meta_t &operator=(const audio_meta_t &x);
    audio_meta_t &operator=(audio_meta_t &&x) = default;

    bool operator==(const audio_meta_t &x) const;
    bool operator!=(const audio_meta_t &x) const;

    operator bool() const {
        return _valid;
    }

    /*
        cid:编码方式
        data:待解析数据
        bytes:待解析数据长度
        brief：解析元数据，并填充aac
    */
    int load(int32_t cid, const uint8_t *data, int bytes);
    /*
        data:specific_config缓冲区
        bytes:填充字节
        brief：用aac元数据填充data
    */
    int save(uint8_t *data, int bytes) const;

    // brief:音频附加头(PacketType==0)+config
    virtual temporary_buffer<uint8_t> to_tag_data() const;

    virtual sstring to_string() const override;

 private:
    /*
        cid:编码方式
        data:待解析数据
        bytes:待解析数据长度
        brief：解析元数据，并填充aac
    */
    int _load(int32_t cid, const uint8_t *data, int bytes);
};

// flv 视频元数据
struct video_meta_t : public meta_t {
 public:
    uint8_t framerate = 0; // fps
    uint32_t width = 0;    // 宽像素
    uint32_t height = 0;   // 高像素
    sstring encoder = "";

    //实际元数据
    union {
        struct aom_av1_t av1;
        struct mpeg4_avc_t avc;
        struct mpeg4_hevc_t hevc;
    } v;

 public:
    video_meta_t();
    video_meta_t(const video_meta_t &x);
    virtual ~video_meta_t() = default;

    video_meta_t &operator=(video_meta_t &&x) = default;
    video_meta_t &operator=(const video_meta_t &x);

    bool operator==(const video_meta_t &x) const;
    bool operator!=(const video_meta_t &x) const;

    operator bool() const {
        return _valid;
    }
    /*
        cid:编码方式
        data:待解析数据
        bytes:待解析数据长度
        brief：解析元数据，并填充v
    */
    int load(int32_t cid, const uint8_t *data, int bytes);
    /*
        data:specific_config缓冲区
        bytes:填充字节
        brief：用v数据填充data
    */
    int save(uint8_t *data, int bytes) const;

    // brief:视频附加头(PacketType==0)+config
    virtual temporary_buffer<uint8_t> to_tag_data() const;

    virtual sstring to_string() const override;

 private:
     /*
        cid:编码方式
        data:待解析数据
        bytes:待解析数据长度
        brief：解析元数据，并填充v
    */
    int _load(int32_t cid, const uint8_t *data, int bytes);
};

// flv 音视频元数据
class metadata_t : public metadata_base<video_meta_t,audio_meta_t> {
public:
    metadata_t() = default;
    metadata_t(const metadata_t &x);
    metadata_t(const metadata_t &x, media_type_t media_type);
    metadata_t(metadata_t &&x) = default;
    metadata_t(const uint8_t *data, size_t len);
    virtual ~metadata_t() = default;

    metadata_t &operator=(metadata_t &&x) = default;
    metadata_t &operator=(const metadata_t &x);

    bool operator==(const metadata_t &x) const;
    bool operator!=(const metadata_t &x) const;

    //解析AMF的buffer
    int load(const uint8_t *data, size_t len);
    //创建AMF的buffer
    virtual temporary_buffer<uint8_t> to_tag_data() const;

    virtual sstring to_string() const override;

    friend std::ostream &operator<<(std::ostream &os, const metadata_t *v) {
        return os << v->to_string();
    }

    friend std::ostream &operator<<(std::ostream &os, const metadata_t &v) {
        return os << &v;
    }
};

struct rtmp_sample_access_t {
    bool arg1 = false;
    bool arg2 = false;

    int decode(const uint8_t *data, size_t len);
};

enum class amf_command : unsigned short {
    unknown,
    meta_data,
    text_data,
    caption,
    caption_info,
    cue_point,
    rtmp_sample_access
};

extern sstring parse_command_name(const uint8_t *data, size_t len);
extern amf_command command_from_name(const sstring &name);

extern int parse_command_name(const uint8_t *data, size_t len, char *out, size_t *out_len);
extern int parse_amf_command(const uint8_t *data, size_t len, amf_command *cmd);

extern int32_t samplerate_from_flv_rate_index(size_t index);
extern int flv_rate_index_from_samplerate(uint32_t samplerate);

extern int32_t samplesize_from_flv_bits_index(size_t index);
extern int flv_bits_index_from_samplesize(uint32_t samplesize);

struct script_t {
    sstring cmd_name;
    temporary_buffer<uint8_t> data;

    script_t() = default;
    script_t(script_t &&s) = default;

    script_t(temporary_buffer<uint8_t> buf);

    virtual ~script_t() = default;
    script_t &operator=(script_t &&s) = default;
};

} // namespace flv

} // namespace amadeus
