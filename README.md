<!--Copyright 2023 Kieran W Harvie. All rights reserved.
	Use of this source code is governed by an MIT-style
	license that can be found in the LICENSE file. -->

# YALE-JIT
This is a proof of concept for some large changes I want to make to the [YALE engine](https://github.com/CleisthenesH/YALE) including:

- LuaJIT compatibility.
- Stronger typing.
- Folding board_manager into the core widget_interface.
	- Changes to upvalues, make it a table.
	- If something isn't reserved it goes into the fenv table.
		- Including init tables.
- Big changes to widget memory layout
	- More use of anonymous structs. 
	- Folding in to tweener.
- Incorporate improved Lua knowledge into the architecture.

These are some pretty big changes so I've decided on a pseudo-rewrite of the engine where I start blank and copy over features one at a time while making the required changes.
I comfort myself through this process by saying:
> Good programmers throw out a lot of code.

Check [the original](https://github.com/CleisthenesH/YALE) for comparison and more info.
