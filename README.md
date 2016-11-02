# Pmilter: Progammable Mail Filter

## milter install and run

```
make mruby
make
```

- run

```
make run
```

or

```
./pmilter -c pmilter.conf
```

### build dependency (for example on ubuntu 16.04)

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

### milter test after `make run`

```
make test
```

## pmilter.conf and mruby handlers

### pmilter.conf using toml

```toml
[server]
# hoge.sock or ipaddree:port
listen = "/var/spool/postfix/pmilter/pmilter.sock"
timeout = 7210
log_level = "notice"
mruby_handler = true
listen_backlog = 128
debug = 0

[handler]
# connection info filter handler
mruby_connect_handler = "handler/connect.rb"

# SMTP HELO command filter handler
mruby_helo_handler = "handler/helo.rb"

# envelope sender filter handler
mruby_envfrom_handler = "handler/mail_from.rb"

# envelope recipient filter handler
mruby_envrcpt_handler = "handler/rcpt_to.rb"

## header filter handler
mruby_header_handler = "handler/header.rb"

# end of header handler
#mruby_eoh_handler = "/path/to/handler.rb"

# body block filter handler
mruby_body_handler = "handler/body.rb"

# end of message handler
mruby_eom_handler = "handler/eom.rb"

# message aborted handler
#mruby_abort_handler = "/path/to/handler.rb"

# connection cleanup handler
#mruby_close_handler = "/path/to/handler.rb"

# unknown SMTP commands handler
#mruby_unknown_handler = "/path/to/handler.rb"

## DATA command handler
#mruby_data_handler = "/path/to/handler.rb"
```

### mruby handler examples

- `handler/connect.rb`

```ruby
puts "hello pmilter handler called from #{Pmilter.name}"
puts "client ipaddr #{Pmilter::Session.new.client_ipaddr}"
puts "client hostname #{Pmilter::Session.new.client_hostname}"
puts "client daemon #{Pmilter::Session.new.client_daemon}"
puts "handler phase name: #{Pmilter::Session.new.handler_phase_name}"
```

- `handler/helo.rb`

```ruby
puts "helo hostname: #{Pmilter::Session.new.helo_hostname}"
puts "tls client issuer: #{Pmilter::Session.new.cert_issuer}"
puts "tls client subject: #{Pmilter::Session.new.cert_subject}"
puts "tls session key size: #{Pmilter::Session.new.cipher_bits}"
puts "tls encrypt method: #{Pmilter::Session.new.cipher}"
puts "tls version: #{Pmilter::Session.new.tls_version}"
```

- `handler/mail_from.rb`

```ruby
puts "env from from args: #{Pmilter::Session.new.envelope_from}"
puts "env from from symval: #{Pmilter::Session.new.mail_addr}"
puts "SASL login name: #{Pmilter::Session.new.auth_authen}"
puts "SASL login sender: #{Pmilter::Session.new.auth_author}"
puts "SASL login type: #{Pmilter::Session.new.auth_type}"

if Pmilter::Session.new.envelope_from == "<spam-from@example.com>"
  Pmilter.status = Pmilter::SMFIS_REJECT
end
```

- `handler/rcpt_to.rb`

```ruby
puts "env to from arg: #{Pmilter::Session.new.envelope_to}"
puts "env to from symval: #{Pmilter::Session.new.rcpt_addr}"
```

- `handler/eom.rb`

```ruby
puts "myhostname: #{Pmilter::Session.new.myhostname}"
puts "message_id: #{Pmilter::Session.new.message_id}"
puts "reveive_time: #{Time.at Pmilter::Session.new.receive_time}"
puts "add_header(X-Pmilter:True): #{Pmilter::Session::Headers.new['X-Pmilter'] = 'Enable'}"
```

- `handler/header.rb`

```ruby
puts "header: #{Pmilter::Session::Headers.new.header}"
```

- `handler/body.rb`

```ruby
puts "body chunk; #{Pmilter::Session.new.body_chunk}"

# Skip over rest of same callbacks
# only once call body handler when return Pmilter::SMFIS_SKIP
Pmilter.status = Pmilter::SMFIS_SKIP
```

### pmilter example handler run

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

## MTA like postfix configuration example

- postfix main.cf

```
# postfix chroot on /var/spool/postfix
# create pmilter.socket as /var/spool/postfix/pmilter/pmilter.sock
smtpd_milters = unix:/pmilter/pmilter.sock
```

## Benchmarks

- don't use pmilter

```
ubuntu@ubuntu-xenial:~$ postal -t 1 -r 10000 -m 1 -M 1 127.0.0.1 mail.list
time,messages,data(K),errors,connections,SSL connections
17:39,83,119,0,84,0
17:40,1245,1774,0,1245,0
17:41,1288,1833,0,1288,0
17:42,1298,1847,0,1298,0
```

- use pmilter and callback mruby handler 10 times

```
ubuntu@ubuntu-xenial:~$ postal -t 1 -r 10000 -m 1 -M 1 127.0.0.1 mail.list
time,messages,data(K),errors,connections,SSL connections
17:45,687,979,0,688,0
17:46,1030,1467,0,1030,0
17:47,1027,1462,0,1027,0
17:48,1042,1483,0,1042,0
```

# License
under the MIT License: see also LICENSE file

* http://www.opensource.org/licenses/mit-license.php

