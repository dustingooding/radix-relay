#include <main_window.h>
#include <slint.h>
#include <spdlog/spdlog.h>

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int /*argc*/, char ** /*argv*/) -> int
{
  spdlog::set_level(spdlog::level::info);

  {
    auto window = MainWindow::create();

    window->set_node_fingerprint("slint-demo");
    window->set_current_mode("demo");

    auto message_model = std::make_shared<slint::VectorModel<Message>>();

    Message welcome;
    welcome.content = slint::SharedString("Hello from Slint!");
    welcome.timestamp = slint::SharedString("00:00:00");
    message_model->push_back(welcome);

    window->set_messages(message_model);

    window->on_send_command([](const slint::SharedString &command) {
      spdlog::info("Command: {}", std::string(command.data(), command.size()));
    });

    window->run();

    message_model.reset();
  }

  return 0;
}
