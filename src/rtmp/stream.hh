#pragma once

#include <seastar/core/iostream.hh>
#include <seastar/core/loop.hh>
#include <seastar/core/queue.hh>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/util/concepts.hh>
#include <deque>

#include "rtmp/exception.hh"
#include "rtmp/packet.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

//取数据的类，声明get方法，获取一个packet
class data_source_impl {
 public:
    virtual ~data_source_impl() {}

    virtual future<packet> get() = 0;

    virtual future<> close() {
        return make_ready_future<>();
    }
};

//对data_source_impl包装一层
class data_source {
    std::unique_ptr<data_source_impl> _dsi;

 protected:
    data_source_impl* impl() const {
        return _dsi.get();
    }

 public:
    data_source() noexcept = default;

    explicit data_source(std::unique_ptr<data_source_impl> dsi) noexcept
    : _dsi(std::move(dsi)) {}

    data_source(data_source&& x) noexcept = default;
    data_source& operator=(data_source&& x) noexcept = default;

    future<packet> get() noexcept {
        try {
            return _dsi->get();
        } catch (...) { return current_exception_as_future<packet>(); }
    }

    future<> close() noexcept {
        try {
            return _dsi->close();
        } catch (...) { return current_exception_as_future<>(); }
    }
};

//存数据的类，声明put方法，将一个packet存起来
class data_sink_impl {
 public:
    virtual ~data_sink_impl() {}

    virtual future<> put(std::vector<packet> pkts) {
        return do_with(std::move(pkts), [this](auto& pkts) {
            return do_for_each(pkts, [this](auto& pkt) {
                return put(std::move(pkt));
            });
        });
    }

    virtual future<> put(packet pkt) = 0;

    virtual future<> flush() {
        return make_ready_future<>();
    }

    virtual future<> close() = 0;
};

//对data_sink_impl包装一层
class data_sink {
    std::unique_ptr<data_sink_impl> _dsi;

 public:
    data_sink() noexcept = default;

    explicit data_sink(std::unique_ptr<data_sink_impl> dsi) noexcept
    : _dsi(std::move(dsi)) {}

    data_sink(data_sink&& x) noexcept = default;
    data_sink& operator=(data_sink&& x) noexcept = default;

    future<> put(std::vector<packet> data) noexcept {
        try {
            return _dsi->put(std::move(data));
        } catch (...) { return current_exception_as_future(); }
    }

    future<> put(packet data) noexcept {
        try {
            return _dsi->put(std::move(data));
        } catch (...) { return current_exception_as_future(); }
    }

    future<> flush() noexcept {
        try {
            return _dsi->flush();
        } catch (...) { return current_exception_as_future(); }
    }

    future<> close() noexcept {
        try {
            return _dsi->close();
        } catch (...) { return current_exception_as_future(); }
    }
};

struct continue_consuming {};

//对packet做封装，可能是最后一个pkt？
class stop_consuming {
 public:
    explicit stop_consuming(packet pkt)
    : _pkt(std::move(pkt)) {}

    packet& get_packet() {
        return _pkt;
    }

    const packet& get_packet() const {
        return _pkt;
    }

 private:
    packet _pkt;
};

//消费结果类，可能是继续消费，也可能是停止消费
class consumption_result {
 public:
    using consumption_variant = std::variant<continue_consuming, stop_consuming>;

    consumption_result(std::optional<packet> opt_pkt) {
        if (opt_pkt) { _result = stop_consuming(std::move(opt_pkt.value())); }
    }

    consumption_result(const continue_consuming&) {}

    consumption_result(stop_consuming&& stop)
    : _result(std::move(stop)) {}

    consumption_variant& get() {
        return _result;
    }

    const consumption_variant& get() const {
        return _result;
    }

 private:
    consumption_variant _result;
};

// Consumer concept, for consume() method
//#define SEASTAR_CONCEPT(x...) x 这只是一个宏，目的是文字替换
//在C++20中新增加了概念（concepts）这一特性，它允许程序员为模板定义预期的约束条件，这些约束说明了模板参数必须满足的属性
//以下的宏定义了两个概念
/*
InputStreamConsumer：类型Consumer的对象c能够执行c(packet())，并且这个表达式的返回类型必须严格是future<consumption_result>类型
ObsoleteInputStreamConsumer：与上一个类似，只不过返回值是future<std::optional<packet>>
*/
SEASTAR_CONCEPT(
    template <typename Consumer> concept InputStreamConsumer =
        requires (Consumer c) {
            { c(packet()) } -> std::same_as<future<consumption_result>>;
        };

    template <typename Consumer> concept ObsoleteInputStreamConsumer =
        requires (Consumer c) {
            { c(packet()) } -> std::same_as<future<std::optional<packet>>>;
        };

)

//接收packet的stream类，并声明消费的接口
class input_stream final {
    data_source _fd;
    packet _pkt;
    bool _eof = false; //_pkt.data.empty()

 private:
    size_t available() const noexcept {
        return _pkt.data.size();
    }

 protected:
    void reset() noexcept {
        _pkt = {};
    }

    data_source* fd() noexcept {
        return &_fd;
    }

 public:
    input_stream() noexcept = default;

    explicit input_stream(data_source fd) noexcept
    : _fd(std::move(fd))
    , _pkt() {}

    input_stream(input_stream&&) = default;
    input_stream& operator=(input_stream&&) = default;

    //用Consumer对_fd进行消费
    template <typename Consumer>
    SEASTAR_CONCEPT(requires InputStreamConsumer<Consumer> || ObsoleteInputStreamConsumer<Consumer>)
    future<> consume(Consumer&& c) noexcept(std::is_nothrow_move_constructible_v<Consumer>);
    template <typename Consumer>
    SEASTAR_CONCEPT(requires InputStreamConsumer<Consumer> || ObsoleteInputStreamConsumer<Consumer>)
    future<> consume(Consumer& c) noexcept(std::is_nothrow_move_constructible_v<Consumer>);

    bool eof() const noexcept {
        return _eof;
    }

    //从_fd中读pkt到_pkt
    future<packet> read() noexcept;

    future<> close() noexcept {
        return _fd.close();
    }

    //后面用 && 代表右值对象也可以用这个函数，移走_fd
    data_source detach() &&;
};

class output_stream final {
    data_sink _fd; //输入流描述符
    std::deque<packet> _pkts; //待入流的包缓冲区

    size_t _size = 0; //暂存包最大数量

    std::exception_ptr _ex; //异常指针

 private:
    //调用_fd.put把pkt放入_fd
    future<> put(packet pkt) noexcept;

    //把_pkts的pkt全部写入_fd
    future<> do_flush() noexcept;

 public:
    output_stream() noexcept = default;

    output_stream(data_sink fd, size_t size) noexcept
    : _fd(std::move(fd))
    , _size(size) {}

    output_stream(data_sink fd) noexcept
    : _fd(std::move(fd)) {}

    output_stream(output_stream&&) noexcept = default;
    output_stream& operator=(output_stream&&) noexcept = default;

    ~output_stream() {
        assert(_pkts.empty() && "Was this stream properly closed?");
    }

    //如果暂存区有空闲空间，则把pkt放入_pkts，否则直接调用_fd.put
    future<> write(packet) noexcept;

    //调用do_flush把_pkts的pkt全部写入_fd
    future<> flush() noexcept;

    future<> close() noexcept;

    data_sink detach() &&;
};

//in.consume(stream_copy_consumer(out))，实际上会把input_stream的pkts拷贝到output_stream
future<> copy(input_stream&, output_stream&);


future<> skip(input_stream&);

//media 消费stream
class media_data_source_impl : public data_source_impl {
    seastar::queue<packet>& _data; //暂存media包
    bool _closed = false; //media数据流是否结束

 public:
    media_data_source_impl(seastar::queue<packet>& data)
    : _data(data) {}

    //从_data中读pkt
    virtual future<packet> get() override {
        if (_closed) return make_ready_future<packet>(packet());
        return _data.pop_eventually();
    }

    virtual future<> close() override {
        if (_closed) return make_ready_future();
        _closed = true;

        return _data.push_eventually(packet());
    }
};

class media_data_sink_impl : public data_sink_impl {
    seastar::queue<packet>& _data; //暂存media包
    bool _closed = false; //media数据流是否结束

 public:
    media_data_sink_impl(seastar::queue<packet>& data)
    : _data(data) {}

    //把pkt放入_data
    virtual future<> put(packet pkt) override {
        // if (_closed) return make_exception_future(std::system_error(ENOTCONN, std::system_category()));
        return _data.push_eventually(std::move(pkt));
    }

    //放入一个空包，表示media数据流结束
    virtual future<> close() override {
        if (_closed) return make_ready_future();
        _closed = true;

        _data.consume([](auto pkt) {
            return true;
        });
        return _data.push_eventually(packet());
    }
};

} // namespace rtmp
} // namespace amadeus
