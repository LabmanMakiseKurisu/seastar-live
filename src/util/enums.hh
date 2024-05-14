#pragma once

#include <boost/algorithm/string.hpp>
#include <seastar/core/seastar.hh>
#include <seastar/util/log.hh>

namespace amadeus {

#define HEADER_ID_NULL LONG_MIN

using namespace seastar;

// 媒体类型
enum class media_type_t : unsigned int {
    none = 0,
    video = 1 << 0,
    audio = 1 << 2,

    all = video | audio
};

// 服务类型
enum class type_t : unsigned int {
    none = 0,

    play = 1 << 0,
    publish = 1 << 1,

    all = play | publish,
};

// 封装类型
enum class format_t : unsigned int {
    UNKNOWN = 0,
    BMT,
    FLV,
    HLS,
    TS,

    ignored
};

// 协议类型
enum class protocol_t : unsigned int {
    none = 0,

    TCP = 1 << 0,
    RTMP = 1 << 1,
    SRT = 1 << 2,
    FILE = 1 << 3,

    HTTP1 = 1 << 4,
    HTTP2 = 1 << 5,
    HTTP3 = 1 << 7,

    HTTP = HTTP1 | HTTP2 | HTTP3,
};

// 所有权
enum class ownership_t : unsigned int {
    ignored = 0,
    internal = 1,
    user = 2,
    invisible = 3
};

static inline const sstring
media_type_to_string(media_type_t t) {
    switch (t) {
        case media_type_t::audio: return "audio";
        case media_type_t::video: return "video";
        case media_type_t::all: return "all";
        default: return "none";
    }
}

static inline const sstring
media_type_to_short_string(media_type_t t) {
    switch (t) {
        case media_type_t::audio: return "a";
        case media_type_t::video: return "v";
        case media_type_t::all: return "av";
        default: return "n";
    }
}

template <class T>
static inline media_type_t
convert_to_media_type(const T &_v) {
    switch (_v) {
        case T::audio: return media_type_t::audio;
        case T::video: return media_type_t::video;
        case T::all: return media_type_t::all;
        default: return media_type_t::none;
    }
}

static inline media_type_t
str2media(const sstring &str) {
    static const sstring arr[] = {"audio", "video", "all"};
    int i;
    for (i = 0; i < 3; i++) {
        if (arr[i] == str) { break; }
    }
    switch (i) {
        case 0: return media_type_t::audio; break;
        case 1: return media_type_t::video; break;
        case 2: return media_type_t::all; break;
        default: return media_type_t::none; break;
    }
}

static inline const sstring
type_to_string(type_t t) {
    switch (t) {
        case type_t::all: return "all";
        case type_t::publish: return "publish";
        case type_t::play: return "play";
        default: return "none";
    }
}

template <class T>
static inline type_t
convert_to_type(const T &_v) {
    switch (_v) {
        case T::publish: return type_t::publish;
        case T::play: return type_t::play;
        case T::all: return type_t::all;
        default: return type_t::all;
    }
}

static inline const sstring
protocol_to_string(protocol_t prot) {
    switch (prot) {
        case protocol_t::TCP: return "TCP";
        case protocol_t::HTTP1: return "HTTP1";
        case protocol_t::HTTP2: return "HTTP2";
        case protocol_t::HTTP3: return "HTTP3";
        case protocol_t::RTMP: return "RTMP";
        case protocol_t::SRT: return "SRT";
        case protocol_t::FILE: return "FILE";
        default: return "";
    }
}

static inline format_t
protocol_to_default_format(protocol_t prot) {
    switch (prot) {
        case protocol_t::TCP: return format_t::BMT;
        case protocol_t::HTTP1: return format_t::FLV;
        case protocol_t::HTTP2: return format_t::BMT;
        case protocol_t::HTTP3: return format_t::BMT;
        case protocol_t::RTMP: return format_t::FLV;
        case protocol_t::SRT: return format_t::BMT;
        case protocol_t::FILE: return format_t::BMT;
        default: return format_t::UNKNOWN;
    }
}

static inline const sstring
protocol_to_schema(protocol_t prot) {
    switch (prot) {
        case protocol_t::TCP: return "bmt";
        case protocol_t::HTTP1: return "http";
        case protocol_t::HTTP2: return "https";
        case protocol_t::HTTP3: return "http3";
        case protocol_t::RTMP: return "rtmp";
        case protocol_t::SRT: return "srt";
        case protocol_t::FILE: return "file";
        default: return "";
    }
}

static inline const sstring
session_type_to_string(type_t type, ownership_t owner) {
    if (type == type_t::publish) {
        if (owner == ownership_t::user) {
            return "publish";
        } else if (owner == ownership_t::internal) {
            return "relay";
        } else {
            return "unknown";
        }
    } else if (type == type_t::play) {
        if (owner == ownership_t::user) {
            return "play";
        } else if (owner == ownership_t::internal) {
            return "relay-player";
        } else {
            return "unknown";
        }
    } else {
        return "unknown";
    }
}

static inline protocol_t
schema_to_protocol(const sstring &schema) {
    auto s = boost::algorithm::to_lower_copy(schema);

    if (s == "bmt") return protocol_t::TCP;
    if (s == "http") return protocol_t::HTTP1;
    if (s == "https") return protocol_t::HTTP2;
    if (s == "http3") return protocol_t::HTTP3;
    if (s == "rtmp") return protocol_t::RTMP;
    if (s == "srt") return protocol_t::SRT;
    if (s == "file") return protocol_t::FILE;
    return protocol_t::none;
}

static inline protocol_t
convert_to_protocol(const int &_v) {
    switch (_v) {
        case 0: return protocol_t::TCP;
        case 1: return protocol_t::HTTP1;
        case 2: return protocol_t::HTTP2;
        case 3: return protocol_t::HTTP3;
        case 4: return protocol_t::RTMP;
        case 5: return protocol_t::SRT;
        case 6: return protocol_t::FILE;
        default: return protocol_t::none;
    }
}

static inline protocol_t
str2protocol(const sstring &str) {
    static const sstring arr[] = {"TCP", "HTTP1", "HTTP2", "HTTP3", "RTMP", "SRT", "FILE"};
    int i;
    for (i = 0; i < 7; i++) {
        if (arr[i] == str) { return convert_to_protocol(i); }
    }
    return convert_to_protocol(i);
}

template <class T>
static inline protocol_t
convert_to_hls_protocol(const T &_v) {
    switch (_v) {
        case T::TCP: return protocol_t::TCP;
        case T::HTTP1: return protocol_t::HTTP1;
        case T::HTTP2: return protocol_t::HTTP2;
        case T::HTTP3: return protocol_t::HTTP3;
        case T::ignored: return protocol_t::none;
        default: return protocol_t::none;
    }
}

static inline const sstring
format_to_string(format_t fmt) {
    switch (fmt) {
        case format_t::BMT: return "BMT";
        case format_t::FLV: return "FLV";
        case format_t::HLS: return "HLS";
        default: return "BMT";
    }
}

static inline const sstring
format_to_schema(format_t fmt) {
    switch (fmt) {
        case format_t::BMT: return "bmt";
        case format_t::FLV: return "flv";
        case format_t::HLS: return "hls";
        default: return "bmt";
    }
}

static inline const sstring
format_to_extension(format_t fmt) {
    auto str = format_to_string(fmt);
    return boost::algorithm::to_lower_copy(str);
}

static inline const sstring
format_to_mime_type(format_t fmt) {
    switch (fmt) {
        case format_t::BMT: return "application/octet-stream";
        case format_t::HLS: return "application/x-mpegURL";
        case format_t::FLV: return "flv-application/octet-stream";
        default: return "application/octet-stream";
    }
}

extern const sstring extension_to_mime_type(sstring extension);

template <class T>
static inline format_t
convert_to_format(const T &_v) {
    switch (_v) {
        case T::BMT: return format_t::BMT;
        case T::FLV: return format_t::FLV;
        case T::HLS: return format_t::HLS;
        default: return format_t::BMT;
    }
}

static inline format_t
str2format(const sstring &str) {
    static const sstring arr[] = {"bmt", "flv", "hls", "ignored"};
    int i;
    for (i = 0; i < 4; i++) {
        if (arr[i] == str) { break; }
    }
    switch (i) {
        case 0: return format_t::BMT;
        case 1: return format_t::FLV;
        case 2: return format_t::HLS;
        default: return format_t::UNKNOWN;
    }
}

template <class T>
static inline format_t
convert_to_format_lower(const T &_v) {
    switch (_v) {
        case T::bmt: return format_t::BMT;
        case T::flv: return format_t::FLV;
        case T::hls: return format_t::HLS;
        case T::ignored: return format_t::ignored;
        default: return format_t::BMT;
    }
}

static inline const sstring
ownership_to_string(ownership_t o) {
    switch (o) {
        case ownership_t::user: return "user";
        case ownership_t::internal: return "internal";
        case ownership_t::invisible: return "invisible";
        case ownership_t::ignored: return "ignored";
        default: return "ignored";
    }
}

static inline const sstring
log_level_to_string(log_level level) {
    switch (level) {
        case log_level::error: return "ERROR";
        case log_level::warn: return "WARN ";
        case log_level::info: return "INFO ";
        case log_level::debug: return "DEBUG";
        case log_level::trace: return "TRACE";
        default: return "INFO ";
    }
}

template <class T>
static inline ownership_t
convert_to_ownership(const T &_v) {
    switch (_v) {
        case T::user: return ownership_t::user;
        case T::internal: return ownership_t::internal;
        case T::invisible: return ownership_t::invisible;
        case T::ignored: return ownership_t::ignored;
        default: return ownership_t::ignored;
    }
}

static inline bool
validate_publish_protocol_and_format(protocol_t prot, format_t fmt) {
    switch (prot) {
        case protocol_t::TCP: return fmt == format_t::BMT;
        case protocol_t::HTTP1: return fmt == format_t::BMT;
        case protocol_t::HTTP2: return fmt == format_t::BMT;
        case protocol_t::HTTP3: return fmt == format_t::BMT;
        case protocol_t::RTMP: return fmt == format_t::FLV;
        case protocol_t::SRT: return fmt == format_t::BMT;
        case protocol_t::FILE: return fmt == format_t::BMT;
        default: return "";
    }
}

static inline bool
validate_play_protocol_and_format(protocol_t prot, format_t fmt) {
    switch (prot) {
        case protocol_t::TCP: return fmt == format_t::BMT;
        case protocol_t::HTTP1:
            return fmt == format_t::BMT || fmt == format_t::FLV || fmt == format_t::HLS || fmt == format_t::TS;
        case protocol_t::HTTP2:
            return fmt == format_t::BMT || fmt == format_t::FLV || fmt == format_t::HLS || fmt == format_t::TS;
        case protocol_t::HTTP3:
            return fmt == format_t::BMT || fmt == format_t::FLV || fmt == format_t::HLS || fmt == format_t::TS;
        case protocol_t::RTMP: return fmt == format_t::FLV;
        case protocol_t::SRT: return fmt == format_t::BMT;
        case protocol_t::FILE: return fmt == format_t::BMT;
        default: return "";
    }
}

inline media_type_t
operator|(media_type_t a, media_type_t b) {
    return media_type_t(std::underlying_type_t<media_type_t>(a) | std::underlying_type_t<media_type_t>(b));
}

inline void
operator|=(media_type_t &a, media_type_t b) {
    a = (a | b);
}

inline media_type_t
operator&(media_type_t a, media_type_t b) {
    return media_type_t(std::underlying_type_t<media_type_t>(a) & std::underlying_type_t<media_type_t>(b));
}

inline void
operator&=(media_type_t &a, media_type_t b) {
    a = (a & b);
}

inline media_type_t
operator~(media_type_t a) {
    return media_type_t(~std::underlying_type_t<media_type_t>(a));
}

inline type_t
operator|(type_t a, type_t b) {
    return type_t(std::underlying_type_t<type_t>(a) | std::underlying_type_t<type_t>(b));
}

inline void
operator|=(type_t &a, type_t b) {
    a = (a | b);
}

inline type_t
operator&(type_t a, type_t b) {
    return type_t(std::underlying_type_t<type_t>(a) & std::underlying_type_t<type_t>(b));
}

inline void
operator&=(type_t &a, type_t b) {
    a = (a & b);
}

inline protocol_t
operator|(protocol_t a, protocol_t b) {
    return protocol_t(std::underlying_type_t<protocol_t>(a) | std::underlying_type_t<protocol_t>(b));
}

inline void
operator|=(protocol_t &a, protocol_t b) {
    a = (a | b);
}

inline protocol_t
operator&(protocol_t a, protocol_t b) {
    return protocol_t(std::underlying_type_t<protocol_t>(a) & std::underlying_type_t<protocol_t>(b));
}

inline void
operator&=(protocol_t &a, protocol_t b) {
    a = (a & b);
}

std::ostream &operator<<(std::ostream &os, const type_t v);
std::ostream &operator<<(std::ostream &os, const format_t v);
std::ostream &operator<<(std::ostream &os, const protocol_t v);
std::ostream &operator<<(std::ostream &os, const ownership_t v);
std::ostream &operator<<(std::ostream &os, const media_type_t v);

} // namespace amadeus
