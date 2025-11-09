#include <catch2/catch_test_macros.hpp>
#include <tui/scroll_state.hpp>

TEST_CASE("scroll_state starts with stick-to-bottom behavior", "[scroll_state][initialization]")
{
  const radix_relay::tui::scroll_state scroll;
  REQUIRE(scroll.should_stick_to_bottom() == true);
}

TEST_CASE("scroll_state responds to wheel up by disabling stick-to-bottom", "[scroll_state][wheel_up]")
{
  radix_relay::tui::scroll_state scroll;

  REQUIRE(scroll.should_stick_to_bottom() == true);

  scroll.handle_wheel_up();

  REQUIRE(scroll.should_stick_to_bottom() == false);
}

TEST_CASE("scroll_state responds to End key by re-enabling stick-to-bottom", "[scroll_state][end_key]")
{
  radix_relay::tui::scroll_state scroll;

  scroll.handle_wheel_up();
  REQUIRE(scroll.should_stick_to_bottom() == false);

  scroll.handle_end_key();
  REQUIRE(scroll.should_stick_to_bottom() == true);
}

TEST_CASE("scroll_state maintains stick-to-bottom after multiple wheel ups", "[scroll_state][multiple_wheel_up]")
{
  radix_relay::tui::scroll_state scroll;

  scroll.handle_wheel_up();
  scroll.handle_wheel_up();
  scroll.handle_wheel_up();

  REQUIRE(scroll.should_stick_to_bottom() == false);
}

TEST_CASE("scroll_state can transition between states multiple times", "[scroll_state][transitions]")
{
  radix_relay::tui::scroll_state scroll;

  REQUIRE(scroll.should_stick_to_bottom() == true);

  scroll.handle_wheel_up();
  REQUIRE(scroll.should_stick_to_bottom() == false);

  scroll.handle_end_key();
  REQUIRE(scroll.should_stick_to_bottom() == true);

  scroll.handle_wheel_up();
  REQUIRE(scroll.should_stick_to_bottom() == false);

  scroll.handle_end_key();
  REQUIRE(scroll.should_stick_to_bottom() == true);
}

TEST_CASE("scroll_state reset_to_bottom restores stick-to-bottom behavior", "[scroll_state][reset]")
{
  radix_relay::tui::scroll_state scroll;

  scroll.handle_wheel_up();
  REQUIRE(scroll.should_stick_to_bottom() == false);

  scroll.reset_to_bottom();
  REQUIRE(scroll.should_stick_to_bottom() == true);
}

TEST_CASE("scroll_state behavior specification", "[scroll_state][behavior]")
{
  SECTION("new messages appear when stuck to bottom")
  {
    const radix_relay::tui::scroll_state scroll;
    REQUIRE(scroll.should_stick_to_bottom() == true);
  }

  SECTION("scrolling up prevents automatic scroll to bottom")
  {
    radix_relay::tui::scroll_state scroll;
    scroll.handle_wheel_up();
    REQUIRE(scroll.should_stick_to_bottom() == false);
  }

  SECTION("End key returns user to auto-scroll mode")
  {
    radix_relay::tui::scroll_state scroll;
    scroll.handle_wheel_up();
    scroll.handle_end_key();
    REQUIRE(scroll.should_stick_to_bottom() == true);
  }
}
