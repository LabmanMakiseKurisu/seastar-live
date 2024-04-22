#include "flv/tag.hh"

#include "flv/flv.hh"
#include "flv/log.hh"

namespace amadeus {
namespace flv {

using namespace seastar;

temporary_buffer<uint8_t>
standard_tag::make_tag_data(
    type_t type, temporary_buffer<uint8_t> tag_body, uint32_t prev_tag_size, int64_t timestamp) {
    std::vector<uint8_t> ptsb(4);
    // tag size
    int rt = ::flv_tag_size_write(ptsb.data(), 4, prev_tag_size);
    if (rt < 0) return temporary_buffer<uint8_t>();

    struct flv_tag_header_t header = {0};
    header.size = tag_body.size();
    header.type = static_cast<uint8_t>(type);
    header.timestamp = timestamp;

    std::vector<uint8_t> thb(FLV_TAG_HEADER_SIZE);
    rt = ::flv_tag_header_write(&header, thb.data(), FLV_TAG_HEADER_SIZE);
    if (rt < 0) return temporary_buffer<uint8_t>();

    temporary_buffer<uint8_t> buf(ptsb.size() + thb.size() + tag_body.size());

    memcpy(buf.get_write(), ptsb.data(), ptsb.size());
    memcpy(buf.get_write() + ptsb.size(), thb.data(), thb.size());
    memcpy(buf.get_write() + ptsb.size() + thb.size(), tag_body.get(), tag_body.size());

    return buf;
}

temporary_buffer<uint8_t>
standard_tag::header_data(bool allow_audio, bool allow_video) {
    temporary_buffer<uint8_t> hb(FLV_HEADER_SIZE);
    int rt = flv_header_write(allow_audio ? 1 : 0, allow_video ? 1 : 0, hb.get_write(), FLV_HEADER_SIZE);
    if (rt < 0) return temporary_buffer<uint8_t>();

    return hb;
}

temporary_buffer<uint8_t>
tag::make_tag_data(type_t type, temporary_buffer<uint8_t> tag_body, int64_t timestamp) {
    struct flv_tag_header_t header = {0};
    header.size = tag_body.size();
    header.type = static_cast<uint8_t>(type);
    header.timestamp = timestamp;

    std::vector<uint8_t> thb(FLV_TAG_HEADER_SIZE);
    int rt = ::flv_tag_header_write(&header, thb.data(), FLV_TAG_HEADER_SIZE);
    if (rt < 0) return temporary_buffer<uint8_t>();

    std::vector<uint8_t> tsb(4);
    // tag size
    rt = ::flv_tag_size_write(tsb.data(), 4, FLV_TAG_HEADER_SIZE + tag_body.size());
    if (rt < 0) return temporary_buffer<uint8_t>();

    temporary_buffer<uint8_t> buf(thb.size() + tag_body.size() + tsb.size());

    std::copy_n(thb.data(), thb.size(), buf.get_write());
    std::copy_n(tag_body.get(), tag_body.size(), buf.get_write() + thb.size());
    std::copy_n(tsb.data(), tsb.size(), buf.get_write() + thb.size() + tag_body.size());

    return buf;
}

temporary_buffer<uint8_t>
tag::header_data(bool allow_audio, bool allow_video) {
    std::vector<uint8_t> hb(FLV_HEADER_SIZE);
    int rt = flv_header_write(allow_audio ? 1 : 0, allow_video ? 1 : 0, hb.data(), FLV_HEADER_SIZE);
    if (rt < 0) return temporary_buffer<uint8_t>();

    std::vector<uint8_t> tsb(4);
    rt = ::flv_tag_size_write(tsb.data(), 4, 0);
    if (rt < 0) return temporary_buffer<uint8_t>();

    temporary_buffer<uint8_t> buf(hb.size() + tsb.size());

    std::copy_n(hb.data(), hb.size(), buf.get_write());
    std::copy_n(tsb.data(), tsb.size(), buf.get_write() + hb.size());

    return buf;
}

} // namespace flv
} // namespace amadeus
