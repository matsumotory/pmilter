puts "body chunk; #{Pmilter::Session.new.body_chunk}"

# Skip over rest of same callbacks
# only once call body handler when return Pmilter::SMFIS_SKIP
Pmilter.status = Pmilter::SMFIS_SKIP
