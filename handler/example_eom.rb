puts "reveive_time: #{Time.at Pmilter::Session.new.receive_time}"
puts "add_header(X-Pmilter:True): #{Pmilter::Session::Headers.new['X-Pmilter'] = 'Enable'}"
