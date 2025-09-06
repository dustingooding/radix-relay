#include <catch2/catch_test_macros.hpp>
#include <string>
#include <tuple>

#include <radix_relay/cli.hpp>

TEST_CASE("InteractiveCli can be constructed", "[cli][construction]")
{
    std::ignore = radix_relay::InteractiveCli{"test-node", "hybrid"};
    REQUIRE(true);
}

TEST_CASE("InteractiveCli command routing works correctly", "[cli][routing]")
{
    SECTION("should_quit identifies quit commands correctly") {
        REQUIRE(radix_relay::InteractiveCli::should_quit("quit") == true);
        REQUIRE(radix_relay::InteractiveCli::should_quit("exit") == true);
        REQUIRE(radix_relay::InteractiveCli::should_quit("q") == true);
        REQUIRE(radix_relay::InteractiveCli::should_quit("help") == false);
        REQUIRE(radix_relay::InteractiveCli::should_quit("") == false);
        REQUIRE(radix_relay::InteractiveCli::should_quit("version") == false);
    }
    
    SECTION("handle_command routes commands correctly") {
        radix_relay::InteractiveCli cli("test-node", "hybrid");
        
        REQUIRE(cli.handle_command("help") == true);
        REQUIRE(cli.handle_command("peers") == true);
        REQUIRE(cli.handle_command("status") == true);
        REQUIRE(cli.handle_command("sessions") == true);
        REQUIRE(cli.handle_command("scan") == true);
        REQUIRE(cli.handle_command("version") == true);
        REQUIRE(cli.handle_command("mode internet") == true);
        REQUIRE(cli.handle_command("mode mesh") == true);
        REQUIRE(cli.handle_command("mode hybrid") == true);
        REQUIRE(cli.handle_command("send alice hello") == true);
        REQUIRE(cli.handle_command("broadcast hello world") == true);
        REQUIRE(cli.handle_command("connect wss://relay.com") == true);
        REQUIRE(cli.handle_command("trust alice") == true);
        REQUIRE(cli.handle_command("verify bob") == true);
        REQUIRE(cli.handle_command("unknown") == false);
        REQUIRE(cli.handle_command("") == false);
    }
}

TEST_CASE("InteractiveCli mode handling works correctly", "[cli][mode]")
{
    radix_relay::InteractiveCli cli("test-node", "hybrid");
    
    SECTION("mode can be switched to valid modes") {
        cli.handle_mode("internet");
        REQUIRE(cli.get_mode() == "internet");
        
        cli.handle_mode("mesh");
        REQUIRE(cli.get_mode() == "mesh");
        
        cli.handle_mode("hybrid");
        REQUIRE(cli.get_mode() == "hybrid");
    }
    
    SECTION("invalid mode is rejected and mode unchanged") {
        const std::string original_mode = cli.get_mode();
        cli.handle_mode("invalid");
        REQUIRE(cli.get_mode() == original_mode);
        
        cli.handle_mode("");
        REQUIRE(cli.get_mode() == original_mode);
        
        cli.handle_mode("random");
        REQUIRE(cli.get_mode() == original_mode);
    }
}

TEST_CASE("InteractiveCli command handlers execute safely", "[cli][handlers]")
{
    radix_relay::InteractiveCli cli("test-node", "hybrid");
    
    SECTION("static handlers do not throw exceptions") {
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_help());
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_peers());
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_status());
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_sessions());
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_scan());
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_version());
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_broadcast("test message"));
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_connect("wss://relay.damus.io"));
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_trust("alice"));
        REQUIRE_NOTHROW(radix_relay::InteractiveCli::handle_verify("alice"));
    }
    
    SECTION("send handler gracefully handles malformed input") {
        REQUIRE_NOTHROW(cli.handle_send("alice hello world"));
        REQUIRE_NOTHROW(cli.handle_send("bob test message with spaces"));
        REQUIRE_NOTHROW(cli.handle_send("charlie"));
        REQUIRE_NOTHROW(cli.handle_send(""));
        REQUIRE_NOTHROW(cli.handle_send("alice"));
    }
}
