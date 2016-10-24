#!/usr/bin/env ruby
# -*- coding: utf-8 -*-
#
# Copyright (C) 2011  Kouhei Sutou <kou@clear-code.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

require "pathname"

test_dir = Pathname.new(__FILE__).dirname.expand_path
top_dir = test_dir.parent.expand_path
lib_dir = top_dir + "lib"

gemfile = top_dir + "Gemfile"
ENV["BUNDLE_GEMFILE"] = gemfile.to_s
require "bundler/setup"

require "test/unit"
require "test/unit/notify"

$LOAD_PATH.unshift(lib_dir.to_s)
$LOAD_PATH.unshift(test_dir.to_s)

ENV["MILTER_ENV"] = "test"
ENV["RUBYOPT"] = nil

exit Test::Unit::AutoRunner.run(true, test_dir)
