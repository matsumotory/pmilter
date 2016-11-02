puts "env from from args: #{Pmilter::Session.new.envelope_from}"
puts "env from from symval: #{Pmilter::Session.new.mail_addr}"
puts "SASL login name: #{Pmilter::Session.new.auth_authen}"
puts "SASL login sender: #{Pmilter::Session.new.auth_author}"
puts "SASL login type: #{Pmilter::Session.new.auth_type}"

if Pmilter::Session.new.envelope_from == "<spam-from@example.com>"
  Pmilter.status = Pmilter::SMFIS_REJECT
end
