/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <terminal/InputGenerator.h>
#include <terminal/Screen.h> // Coordinate

#include <fmt/format.h>

#include <functional>
#include <vector>
#include <utility>

namespace terminal {

/**
 * Selector API.
 *
 * A Selector can select a range of text. The range can be linear with partial start/end lines, or full lines,
 * or a block based selector, that is capable of selecting all lines partially.
 *
 * The Selector operates on the Screen by accumulating a scrolling offset, that determines
 * the view port of that Screen.
 *
 * When the screen is being modified while selecting text, the selection regions must be preserved,
 * that is, when the selection start is inside the screen, then new lines are added, which causes the screen
 * to move the screen contents up, then also the selection's begin (and extend) is being moved up.
 *
 * This is achieved by using absolute coordinates from the top history line.
 */
class Selector {
  public:
    enum class State {
        /// Inactive, but waiting for the selection to be started (by moving the cursor).
        Waiting,
        /// Active, with selection in progress.
        InProgress,
        /// Inactive, with selection available.
        Complete,
    };

    struct Range {
        cursor_pos_t line;
        cursor_pos_t fromColumn;
        cursor_pos_t toColumn;

        constexpr cursor_pos_t length() const noexcept { return toColumn - fromColumn + 1; }
    };


    using Renderer = Screen::Renderer;
    enum class Mode { Linear, LinearWordWise, FullLine, Rectangular };
	using GetCellAt = std::function<Screen::Cell const&(Coordinate const&)>;

    Selector(Mode _mode,
			 GetCellAt _at,
			 std::u32string const& _wordDelimiters,
			 cursor_pos_t _totalRowCount,
			 WindowSize const& _viewport,
			 Coordinate const& _from);

	/// Convenience constructor when access to Screen is available.
    Selector(Mode _mode,
			 std::u32string const& _wordDelimiters,
			 Screen const& _screen,
			 Coordinate const& _from) :
		Selector{
			_mode,
			std::bind(&Screen::absoluteAt, _screen, std::placeholders::_1),
			_wordDelimiters,
			_screen.size().rows + static_cast<cursor_pos_t>(_screen.historyLineCount()),
			_screen.size(),
			_from
		}
	{
	}

    /// Tests whether the a selection is currently in progress.
    constexpr State state() const noexcept { return state_; }

    /// @todo Should be able to handle negative (or 0) and overflow coordinates,
    ///       which should potentially adjust the screen's view (aka. modifying scrolling offset).
    ///
    /// @retval true TerminalView requires scrolling offset adjustments.
    /// @retval false TerminalView's scrolling offset does not need adjustments.
    bool extend(Coordinate const& _to);

    /// Marks the selection as completed.
    void stop();

    /// When screen lines are sliced into or out of the saved lines buffer, this call will update
    /// the selection accordingly.
    void slice(int _offset);

    constexpr WindowSize const& viewport() const noexcept { return viewport_; }
    constexpr Coordinate const& from() const noexcept { return from_; }
    constexpr Coordinate const& to() const noexcept { return to_; }

    /// Tests whether selection is upwards.
    constexpr bool negativeSelection() const noexcept { return to_ < from_; }
    constexpr bool singleLineSelection() const noexcept { return from_.row == to_.row; }

    constexpr void swapDirection() noexcept
    {
        swap(from_, to_);
    }

	/// Retrieves a vector of ranges (with one range per line) of selected cells.
	std::vector<Range> selection() const;

	/// Constructs a vector of ranges for a linear selection strategy.
	std::vector<Range> linear() const;

	/// Constructs a vector of ranges for a full-line selection strategy.
	std::vector<Range> lines() const;

	/// Constructs a vector of ranges for a rectangular selection strategy.
	std::vector<Range> rectangular() const;

	/// Renders the current selection into @p _render.
	void render(Renderer const& _render);

  private:
	bool isWordWiseSelection() const noexcept
	{
		switch (mode_)
		{
			case Mode::LinearWordWise:
				return true;
			default:
				return false;
		}
	}

	Screen::Cell const& at(Coordinate const& _coord) const { return getCellAt_(_coord); }

	void extendSelectionBackward();
	void extendSelectionForward();

  private:
    State state_{State::Waiting};
	Mode mode_;
	GetCellAt getCellAt_;
	std::u32string wordDelimiters_;
	cursor_pos_t totalRowCount_;
    WindowSize const viewport_;
    Coordinate start_{};
    Coordinate from_{};
    Coordinate to_{};
};

} // namespace terminal

namespace fmt {
    template <>
    struct formatter<terminal::Selector> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx)
        {
            return ctx.begin();
        }

        template <typename FormatContext>
        auto format(const terminal::Selector& _selector, FormatContext& ctx)
        {
            return format_to(ctx.out(), "({} .. {}; state: {})",
                    _selector.from(), _selector.to(), static_cast<int>(_selector.state()));
        }
    };
}

