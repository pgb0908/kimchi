#include "config/loader.h"
#include "server/admin_server.h"
#include "server/gateway_server.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "config/models.h"
#include "handler/admin_handler.h"

DEFINE_string(config_dir, "/etc/kimchi/config",
              "Directory containing JSON config files");
DEFINE_int32(data_port_http, 18000, "HTTP data plane port");
DEFINE_int32(admin_port, 18001, "Admin API port");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    LOG(INFO) << "kimchi gateway starting";
    LOG(INFO) << "Loading config from: " << FLAGS_config_dir;

    auto store = kimchi::config::ConfigLoader::loadFromDirectory(FLAGS_config_dir);

    auto sharedConfig = std::make_shared<kimchi::SharedConfig>();
    sharedConfig->current =
        std::make_shared<kimchi::config::ConfigStore>(std::move(store));

    std::vector<kimchi::config::ListenerConfig> listeners =
        sharedConfig->current->listeners;

    if (listeners.empty()) {
        LOG(WARNING) << "No Listener resources in config; "
                     << "using default HTTP on port " << FLAGS_data_port_http;
        kimchi::config::ListenerConfig def;
        def.protocol = "HTTP";
        def.port = static_cast<uint16_t>(FLAGS_data_port_http);
        def.host = "0.0.0.0";
        def.metadata.name = "default";
        listeners.push_back(std::move(def));
    }

    kimchi::AdminServer admin(static_cast<uint16_t>(FLAGS_admin_port),
                              FLAGS_config_dir, sharedConfig);

    kimchi::GatewayServer gateway(listeners, sharedConfig->current);

    admin.start([] { LOG(INFO) << "Admin API ready"; });
    gateway.start([] { LOG(INFO) << "Data plane ready"; });

    gateway.join();

    admin.stop();
    admin.join();

    google::ShutdownGoogleLogging();
    return 0;
}
