p = Pmilter::Session.new
h = Pmilter::Session::Headers.new

puts "myhostname: #{p.myhostname}"
puts "message_id: #{p.message_id}"
puts "reveive_time: #{Time.at p.receive_time}"
puts "add_header(X-Pmilter:True): #{h['X-Pmilter'] = 'Enable'}"

if p.envelope_from == "<change-from@example.com>"
  p.change_envelope_from "<new-from@example.com>"
end

if p.envelope_to == "<add-to@example.com>"
  p.add_envelope_to "<new-to@example.com>"
end
