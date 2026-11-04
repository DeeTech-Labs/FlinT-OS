#include <cstdlib>
#include <flint/core.h>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace flint;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static Bytes file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
static std::string text(const FlintRender::Scene &scene) {
    return {reinterpret_cast<const char *>(scene.data), scene.used};
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    std::string root = argv[1];
    uint64_t tick = 1000;
    Platform platform;
    platform.build_id = "host-tests";
    platform.monotonic = [&] { return tick; };
    platform.free_storage = [] { return uint64_t(2 * 1024 * 1024); };
    platform.capabilities |= CapResources;
    Core core(root + "/store", root + "/factory", platform);
    CHECK(core.begin() == Error::ok);
    CHECK(core.packages().list().size() == 6);
    Session session{true, false, 7};
    uint32_t sequence = 0;
    auto request = [&](Method method, Bytes data = Bytes{}, Bytes *result = nullptr) {
        auto reply = core.handle({method, ++sequence, false, std::move(data)}, session);
        Reader r{reply.payload};
        auto error = Error(r.n(2));
        CHECK(r.valid);
        if (result)
            result->assign(reply.payload.begin() + 2, reply.payload.end());
        return error;
    };
    CHECK(request(Method::info) == Error::incompatible_contract);
    auto hello = [&](uint32_t revision) {
        Writer w;
        w.n(1, 2);
        w.n(revision);
        w.text("tests");
        w.n(CapPackages, 8);
        return request(Method::hello, w.data);
    };
    CHECK(hello(0) == Error::incompatible_contract);
    CHECK(hello(ContractRevision) == Error::ok);
    session.authenticated = false;
    CHECK(request(Method::info) == Error::permission_denied);
    session.authenticated = true;
    Bytes info;
    CHECK(request(Method::info, {}, &info) == Error::ok);
    FlintRender::Scene scene;
    CHECK(core.scene(scene) == Error::ok);
    CHECK(text(scene).find("--:--") != std::string::npos);
    Writer epoch;
    epoch.n(1793740000, 8);
    CHECK(request(Method::time_set, epoch.data) == Error::ok);
    CHECK(core.now() == 1793740000);
    auto begin = [&](const Bytes &bytes) {
        Sha256 sha;
        sha.update(bytes);
        std::string digest;
        const char *hex = "0123456789abcdef";
        for (auto b : sha.finish()) {
            digest += hex[b >> 4];
            digest += hex[b & 15];
        }
        Writer w;
        w.n(bytes.size());
        w.text(digest);
        Bytes reply;
        CHECK(request(Method::install_begin, w.data, &reply) == Error::ok);
        Reader r{reply};
        return uint32_t(r.n());
    };
    auto chunk = [&](const Bytes &data, uint32_t offset, uint32_t count) {
        Writer w;
        w.n(offset);
        w.raw(std::span(data).subspan(offset, count));
        return request(Method::install_chunk, w.data);
    };
    auto v1 = file(root + "/v1.ignit");
    CHECK(begin(v1) == 0);
    CHECK(chunk(v1, 0, 100) == Error::ok);
    CHECK(chunk(v1, 0, 100) == Error::ok);
    CHECK(core.tick());
    CHECK(!core.tick());
    core.disconnect(session);
    CHECK(core.tick());
    session = {true, false, 8};
    CHECK(hello(ContractRevision) == Error::ok);
    CHECK(begin(v1) == 100);
    CHECK(chunk(v1, 101, 10) == Error::invalid_argument);
    CHECK(chunk(v1, 100, v1.size() - 100) == Error::ok);
    CHECK(request(Method::install_commit) == Error::ok);
    CHECK(core.packages().list().size() == 7);
    CHECK(core.launch("test.monitor", "main") == Error::ok);
    CHECK(core.scene(scene) == Error::ok);
    CHECK(text(scene).find("CPU") == std::string::npos);
    Writer metrics;
    metrics.n(1234);
    metrics.n(5600);
    CHECK(request(Method::metrics, metrics.data) == Error::permission_denied);
    CHECK(core.scene(scene) == Error::ok);
    CHECK(text(scene).find("CPU") == std::string::npos);
    auto grant = [&](bool enabled) {
        Writer w;
        w.text("test.monitor");
        w.text("companion.pc.metrics");
        w.text("cpu,ram");
        w.n(enabled, 1);
        return request(Method::permission_set, w.data);
    };
    CHECK(grant(true) == Error::ok);
    CHECK(request(Method::metrics, metrics.data) == Error::ok);
    CHECK(core.scene(scene) == Error::ok);
    CHECK(text(scene).find("CPU 12.3%") != std::string::npos);
    CHECK(grant(false) == Error::ok);
    CHECK(core.scene(scene) == Error::ok);
    CHECK(text(scene).find("CPU") == std::string::npos);
    CHECK(grant(true) == Error::ok);
    auto install = [&](const std::string &path) {
        auto bytes = file(path);
        auto offset = begin(bytes);
        CHECK(chunk(bytes, offset, bytes.size() - offset) == Error::ok);
        return request(Method::install_commit);
    };
    CHECK(install(root + "/bad-canvas.ignit") == Error::invalid_package);
    CHECK(install(root + "/v2.ignit") == Error::ok);
    Installed current;
    CHECK(core.packages().locate("test.monitor", current) == Error::ok);
    CHECK(current.manifest.version == 2);
    CHECK(core.scene(scene) == Error::ok);
    CHECK(text(scene).find("CPU") == std::string::npos);
    CHECK(install(root + "/spoof.ignit") == Error::permission_denied);
    CHECK(core.packages().locate("system.clock", current) == Error::ok);
    CHECK(current.factory && current.manifest.version == 1);
    // Simulate power-loss/corruption with a valid previous reference and orphaned temporary files.
    core.runtime().stop();
    CHECK(core.packages().locate("test.monitor", current) == Error::ok);
    const auto damaged = current.path;
    const Bytes broken{0, 1, 2, 3};
    CHECK(writeAtomic(damaged, broken));
    {
        std::ofstream temporary(root + "/store/interrupted.ref.tmp");
        temporary << "uncommitted";
    }
    {
        Core rebooted(root + "/store", root + "/factory", platform);
        CHECK(rebooted.begin() == Error::ok);
        Installed restored;
        CHECK(rebooted.packages().locate("test.monitor", restored) == Error::ok);
        CHECK(restored.manifest.version == 1);
        CHECK(readText(root + "/store/interrupted.ref.tmp").empty());
        CHECK(rebooted.runtime().manifest().id == "system.clock");
    }
    Writer rollback;
    rollback.text("test.monitor");
    CHECK(request(Method::rollback, rollback.data) == Error::ok);
    CHECK(core.packages().locate("test.monitor", current) == Error::ok);
    CHECK(current.manifest.version == 1);
    CHECK(install(root + "/resources.ignit") == Error::ok);
    CHECK(core.launch("test.resources", "main") == Error::ok);
    CHECK(core.scene(scene) == Error::ok);
    auto handle = core.runtime().resources().handle(5);
    std::array<uint8_t, 8> header{};
    CHECK(core.runtime().resources().read(handle, 0, header) == Error::ok);
    std::vector<uint16_t> pixels(280 * 240);
    FlintRender::Canvas canvas(pixels.data(), 280, 240);
    core.paint(scene, canvas);
    CHECK(pixels[4 * 280 + 4] == FlintRender::rgb(240, 40, 20));
    CHECK(FlintRender::pixelHash(canvas) != 0);
    CHECK(core.launch("system.clock", "main") == Error::ok);
    CHECK(core.runtime().resources().read(handle, 0, header) == Error::stale_handle);
    for (int i = 0; i < 200; ++i) {
        CHECK(core.launch(i % 2 ? "system.clock" : "test.monitor", "main") == Error::ok);
        CHECK(core.scene(scene) == Error::ok);
    }
    Writer remove;
    remove.text("test.monitor");
    CHECK(request(Method::remove, remove.data) == Error::ok);
    CHECK(core.runtime().manifest().id == "system.clock");
    CHECK(core.packages().locate("test.monitor", current) == Error::source_unavailable);
    CHECK(request(Method::recover) == Error::ok);
    CHECK(core.runtime().manifest().id == "system.clock");
    // Framing: fragmentation, noise, invalid CRC and concatenated frames.
    Frame original{Method::info, 42, false, {1, 2, 3}};
    auto wire = encodeFrame(original);
    FrameParser parser;
    unsigned frames = 0;
    auto receive = [&](Frame f) {
        CHECK(f.id == 42 && f.payload == original.payload);
        ++frames;
    };
    Bytes noisy{0, 1, 2, 3};
    noisy.insert(noisy.end(), wire.begin(), wire.end());
    for (auto byte : noisy)
        parser.feed({&byte, 1}, receive);
    CHECK(frames == 1);
    auto corrupt = wire;
    corrupt.back() ^= 128;
    parser.feed(corrupt, receive);
    parser.feed(wire, receive);
    CHECK(frames == 2);
    EventBus bus;
    for (int i = 0; i < 32; ++i)
        CHECK(bus.post({uint32_t(i)}) == Error::ok);
    CHECK(bus.post({}) == Error::quota_exceeded);
    Event e;
    for (int i = 0; i < 32; ++i) {
        CHECK(bus.poll(e));
        CHECK(e.type == unsigned(i));
    }
    CHECK(!bus.poll(e));
    std::cout << "Core installation/resume/update/rollback, consent/revocation, factory "
                 "protection, external resources, lifecycle and binary framing: passed\n";
}
