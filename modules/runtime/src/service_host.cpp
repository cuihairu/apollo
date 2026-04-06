#include "apollo/runtime/service_host.hpp"

namespace apollo::runtime {

void ServiceHost::add_service(std::shared_ptr<IHostedService> service) {
    application_host_.add_service(std::move(service));
}

void ServiceHost::add_shutdown_hook(ApplicationHost::ShutdownHook hook) {
    application_host_.add_shutdown_hook(std::move(hook));
}

void ServiceHost::set_console_source(std::unique_ptr<IConsoleEventSource> console_source) {
    application_host_.set_console_source(std::move(console_source));
}

void ServiceHost::set_signal_source(std::unique_ptr<ISignalSource> signal_source) {
    application_host_.set_signal_source(std::move(signal_source));
}

bool ServiceHost::start() {
    return application_host_.start();
}

int ServiceHost::run() {
    return application_host_.run();
}

int ServiceHost::run_once() {
    return application_host_.run_once();
}

void ServiceHost::request_stop(StopReason reason) {
    application_host_.request_stop(reason);
}

void ServiceHost::stop() {
    application_host_.stop();
}

apollo::core::ApplicationPhase ServiceHost::phase() const {
    return application_host_.phase();
}

StopReason ServiceHost::stop_reason() const {
    return application_host_.stop_reason();
}

bool ServiceHost::is_running() const {
    return application_host_.is_running();
}

} // namespace apollo::runtime
