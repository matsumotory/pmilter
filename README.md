# [PoC] pmilter

## milter start

```
gcc pmilter.c -lmilter -lpthread -o pmilter
./pmilter -p hoge.sock
```

## milter test

```
cd milter-test-sample
bundle install --path vendor/bundle
./test/run-test.rb
```

- pmilter output debug meesages

```
ubuntu@ubuntu-xenial:~/pmilter$ ./pmilter -p hoge.sock
ipaddr: 192.168.123.123
connect_daemon: milter-test-server
if_name: localhost
if_addr: 127.0.0.1
j: mail.example.com
_: (null)
tls_version: 0
cipher: 0
cipher_bits: 0
cert_subject: cert_subject
cert_issuer: cert_issuer
envelope_from; <from@example.com>
i: i
auth_type: (null)
auth_authen: (null)
auth_ssf: (null)
auth_author: (null)
mail_mailer: mail_mailer
mail_host: mail_host
mail_addr: mail_addr
envelope_to: <to@example.com>
rcpt_mailer: rcpt_mailer
rcpt_host: rcpt_host
rcpt_addr: <to@example.com>
receive_time: 1477302874
msg_id: (null)
------------
ipaddr: 192.168.123.123
connect_daemon: milter-test-server
if_name: localhost
if_addr: 127.0.0.1
j: mail.example.com
_: (null)
tls_version: 0
cipher: 0
cipher_bits: 0
cert_subject: cert_subject
cert_issuer: cert_issuer
envelope_from; <from@example.com>
i: i
auth_type: (null)
auth_authen: (null)
auth_ssf: (null)
auth_author: (null)
mail_mailer: mail_mailer
mail_host: mail_host
mail_addr: mail_addr
envelope_to: <to@example.com>
rcpt_mailer: rcpt_mailer
rcpt_host: rcpt_host
rcpt_addr: <to@example.com>
receive_time: 1477302874
msg_id: (null)
```
