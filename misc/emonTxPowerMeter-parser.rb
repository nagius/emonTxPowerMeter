#!/usr/bin/env ruby

# emonTxPowerMeter-parser.rb - Parser for emonTxPowerMeter serial data
# Copyleft 2019 - Nicolas AGIUS <nicolas.agius@lps-it.fr>

###########################################################################
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
#
###########################################################################


# This script read and parse the ouput of the emonTxPowerMeter arduino firmware 
# from serial port and caluclate the average values of the past 5 minutes.
# The result is written to the OUTPUT file in a bash-readable format. It's a good 
# idea to run it as a daemon.
# This 5 min average is designed to be fed into an RRD system for graphing

require 'serialport'
require 'json'
require 'active_support/core_ext/integer/time'

PORT="/dev/ttyACM0"
SPEED=115200
OUTPUT="/run/shm/emontx"

arduino = SerialPort.new(PORT, SPEED, 8, 1, SerialPort::NONE)
data_celcius = {}
data_humidity = {}
data_power = {}

# Monkey patch to add average() to the Array class
class Array
	def average
		inject(&:+) / size
	end
end

# Infinite loop, this script should be daemonized
while true do
	begin
		data=JSON.parse(arduino.readline("\n"))
	rescue JSON::ParserError
		# Do nothing with the lines which are not JSON
		next
	end

	if data.has_key?('celcius')
		data_celcius[Time.now]=data['celcius']
	end

	if data.has_key?('humidity')
		data_humidity[Time.now]=data['humidity']
	end

	if data.has_key?('power')
		data_power[Time.now]=data['power']
	end
	
	# Remove data older than 5 minutes
	data_celcius.delete_if { |k,v| k < 5.minutes.ago }
	data_humidity.delete_if { |k,v| k < 5.minutes.ago }
	data_power.delete_if { |k,v| k < 5.minutes.ago }

	# Default value to new array
	celcius = Hash.new{ |hash, key| hash[key] = [] }
	humidity = Hash.new{ |hash, key| hash[key] = [] }
	power = Hash.new{ |hash, key| hash[key] = [] }

	# Extract data into flat array
	data_celcius.each_value { |v|
		v.each { |id,value|
			celcius[id] << value
		}
	}
	
	data_humidity.each_value { |v|
		v.each { |id,value|
			humidity[id] << value
		}
	}

	data_power.each_value { |v|
		v.each { |id,value|
			power[id] << value['realPower']
		}
	}

	File.open(OUTPUT, 'w') { |f|
		f.puts "TS=#{Time.now.to_i}"

		# Compute average on past 5 minutes
		celcius.each { |id,values|
			avg = values.average.round(2)
			f.puts "T#{id.tr('-','_')}=#{avg}"
		}

		humidity.each { |id,values|
			avg = values.average.round
			f.puts "H#{id.tr('-','_')}=#{avg}"
		}

		power.each { |id,values|
			avg = values.average.round(2)
			f.puts "#{id}=#{avg}"
		}
	}
end

# vim: ts=2:sw=2:ai
