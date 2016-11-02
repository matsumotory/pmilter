puts "env from: #{Pmilter::Session.new.envelope_from}"
puts "env to: #{Pmilter::Session.new.envelope_to}"
puts "SASL login name: #{Pmilter::Session.new.auth_authen}"
puts "SASL login sender: #{Pmilter::Session.new.auth_author}"
puts "SASL login type: #{Pmilter::Session.new.auth_type}"

if Pmilter::Session.new.envelope_from == "<spam-from@example.com>"
  Pmilter.status = Pmilter::SMFIS_REJECT
end
