-- Copyright 2023-2024 Kieran W Harvie. All rights reserved.
-- Use of this source code is governed by an MIT-style
-- license that can be found in the LICENSE file.

-- Runs after lua has been initalized but before anything else.
-- Used to config things like the display size/thread pool size.

-- Current supported settings:
--	boot_file: the file path for an alternative boot file
--	video_adapter: which video adapter will be used to create the display
--	windowed: whether or not the display is windowed

print("Config Complete")
