# Pmilter: Progammable Mail Filter

## dependency (on ubuntu 16.04)

- build essential
- automake
- m4
- autoreconf
- autoconf
- libtool
- cmake
- pkg-config
- libcunit1-dev
- ragel
- ruby (for mruby)
- bison (for mruby)

## milter install and run

```
make mruby
make
make run
```

## milter test after `make run`

```
make test
```

## pmilter.conf using toml

```toml
[server]
# hoge.sock or ipaddree:port
#listen = "/var/spool/postfix/pmilter/pmilter.sock"
listen = "pmilter.sock"
timeout = 7210
log_level = "notice"
mruby_handler = true
listen_backlog = 128
debug = 0

[handler]
# connection info filter handler
mruby_connect_handler = "handler/example.rb"

# SMTP HELO command filter handler
mruby_helo_handler = "handler/example_helo.rb"

# envelope sender filter handler
mruby_envfrom_handler = "handler/mail_from.rb"

# envelope recipient filter handler
mruby_envrcpt_handler = "handler/rcpt_to.rb"

## header filter handler
mruby_header_handler = "handler/example_header.rb"

## end of header handler
#mruby_eoh_handler = "/path/to/handler.rb"

# body block filter handler
mruby_body_handler = "handler/body.rb"

# end of message handler
mruby_eom_handler = "handler/example_eom.rb"

## message aborted handler
#mruby_abort_handler = "/path/to/handler.rb"
#
## connection cleanup handler
#mruby_close_handler = "/path/to/handler.rb"
#
## unknown SMTP commands handler
#mruby_unknown_handler = "/path/to/handler.rb"
#
## DATA command handler
#mruby_data_handler = "/path/to/handler.rb"
```

### handler example

```ruby
handler/body.rb:puts "body chunk; #{Pmilter::Session.new.body_chunk}"
handler/body.rb:
handler/body.rb:# Skip over rest of same callbacks
handler/body.rb:# only once call body handler when return Pmilter::SMFIS_SKIP
handler/body.rb:Pmilter.status = Pmilter::SMFIS_SKIP
handler/example_eom.rb:puts "myhostname: #{Pmilter::Session.new.myhostname}"
handler/example_eom.rb:puts "message_id: #{Pmilter::Session.new.message_id}"
handler/example_eom.rb:puts "reveive_time: #{Time.at Pmilter::Session.new.receive_time}"
handler/example_eom.rb:puts "add_header(X-Pmilter:True): #{Pmilter::Session::Headers.new['X-Pmilter'] = 'Enable'}"
handler/example_header.rb:puts "header: #{Pmilter::Session::Headers.new.header}"
handler/example_helo.rb:puts "helo hostname: #{Pmilter::Session.new.helo_hostname}"
handler/example_helo.rb:puts "tls client issuer: #{Pmilter::Session.new.cert_issuer}"
handler/example_helo.rb:puts "tls client subject: #{Pmilter::Session.new.cert_subject}"
handler/example_helo.rb:puts "tls session key size: #{Pmilter::Session.new.cipher_bits}"
handler/example_helo.rb:puts "tls encrypt method: #{Pmilter::Session.new.cipher}"
handler/example_helo.rb:puts "tls version: #{Pmilter::Session.new.tls_version}"
handler/example_helo.rb:
handler/example.rb:puts "hello pmilter handler called from #{Pmilter.name}"
handler/example.rb:puts "client ipaddr #{Pmilter::Session.new.client_ipaddr}"
handler/example.rb:puts "client hostname #{Pmilter::Session.new.client_hostname}"
handler/example.rb:puts "client daemon #{Pmilter::Session.new.client_daemon}"
handler/example.rb:puts "handler phase name: #{Pmilter::Session.new.handler_phase_name}"
handler/mail_from.rb:puts "env from from args: #{Pmilter::Session.new.envelope_from}"
handler/mail_from.rb:puts "env from from symval: #{Pmilter::Session.new.mail_addr}"
handler/mail_from.rb:puts "SASL login name: #{Pmilter::Session.new.auth_authen}"
handler/mail_from.rb:puts "SASL login sender: #{Pmilter::Session.new.auth_author}"
handler/mail_from.rb:puts "SASL login type: #{Pmilter::Session.new.auth_type}"
handler/mail_from.rb:
handler/mail_from.rb:if Pmilter::Session.new.envelope_from == "<spam-from@example.com>"
handler/mail_from.rb:  Pmilter.status = Pmilter::SMFIS_REJECT
handler/mail_from.rb:end
handler/rcpt_to.rb:puts "env to from arg: #{Pmilter::Session.new.envelope_to}"
handler/rcpt_to.rb:puts "env to from symval: #{Pmilter::Session.new.rcpt_addr}"
```


### pmilter output debug meesages

- `make test` after `make run`

```
ubuntu@ubuntu-xenial:~/pmilter$ make run
./pmilter -c pmilter.conf
[Wed, 02 Nov 2016 12:02:11 GMT][notice]: pmilter starting
hello pmilter handler called from pmilter
client ipaddr 192.168.123.123
client hostname mx.example.net
client daemon milter-test-server
handler phase name: mruby_connect_handler
helo hostname: delian
tls client issuer: cert_issuer
tls client subject: cert_subject
tls session key size: 0
tls encrypt method: 0
tls version: 0
env from from args: <from@example.com>
env from from symval: mail_addr
SASL login name:
SASL login sender:
SASL login type:
env to from arg: <to@example.com>
env to from symval: <to@example.com>
header: {"From"=>"<from@example.com>"}
header: {"To"=>"<to@example.com>"}
header: {"Subject"=>"Hello"}
body chunk; Hello world!!
myhostname: mail.example.com
message_id: message-id
reveive_time: Wed Nov 02 21:02:15 2016
add_header(X-Pmilter:True): Enable
```

# License
under the MIT License: see also LICENSE file

* http://www.opensource.org/licenses/mit-license.php

