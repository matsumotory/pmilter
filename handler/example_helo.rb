puts "helo hostname: #{Pmilter::Session.new.helo_hostname}"
puts "tls client issuer: #{Pmilter::Session.new.cert_issuer}"
puts "tls client subject: #{Pmilter::Session.new.cert_subject}"
puts "tls session key size: #{Pmilter::Session.new.cipher_bits}"
puts "tls encrypt method: #{Pmilter::Session.new.cipher}"
puts "tls version: #{Pmilter::Session.new.tls_version}"

