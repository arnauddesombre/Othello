@echo off
goto begin

*****************
Othello by Arnaud
*****************

Use keyboard keys:
Arrows, Ctrl-Arrows, Enter, Space Bar, F1 (help)

syntax:
othello <board_size> <player_start> <processors> <monte_carlo> <display_score> <input_mode> <display_modifs>

<board_size>		the size of the board					(default = 8, must be an even number >= 4)
<player_start>		player starts the game (YES/NO)				(default = YES)
<processors>		number of core processors to use			(default = 4, cannot be lower than 1)
<monte_carlo>		cumulated number of Monte Carlo paths for computer's AI	(default = 20000, cannot be lower than 100)
<display_score>		display score assessed on last computer's move (YES/NO)	(default = YES)
<input_mode>		K:Keyboard, M:mouse, B:both (K/M/B)			(default = B)
<display_modifs>	display parameters screen on startup (YES/NO)		(default = YES)

Update parameters below to change the defaults

*****************

:begin
set board_size=8
set player_start=YES
set processors=4
set monte_carlo=20000
set display_score=YES
set input_mode=B
set display_modifs=YES

othello %board_size% %player_start% %processors% %monte_carlo% %display_score% %input_mode% %display_modifs%

set board_size=
set player_start=
set processors=
set monte_carlo=
set display_score=
set input_mode=
set display_modifs=
set display_modifs=