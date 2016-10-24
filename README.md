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

### pmilter output debug meesages

```
ubuntu@ubuntu-xenial:~/pmilter$ ./pmilter -p hoge.sock
------------
mrb_xxfi_negotiate
mrb_xxfi_connect
    hostname: mx.example.net
    info->ipaddr: 192.168.123.123
    info->connect_daemon: milter-test-server
    if_name: localhost
    if_addr: 127.0.0.1
    j: mail.example.com
    _: (null)
mrb_xxfi_helo
    helohost: delian
    tls_version: 0
    cipher: 0
    cipher_bits: 0
    cert_subject: cert_subject
    cert_issuer: cert_issuer
mrb_xxfi_envfrom
    info->envelope_from: <from@example.com>
    i: i
    auth_type: (null)
    auth_authen: (null)
    auth_ssf: (null)
    auth_author: (null)
    mail_mailer: mail_mailer
    mail_host: mail_host
    mail_addr: mail_addr
mrb_xxfi_envrcpt
    info->envelope_to: <to@example.com>
    argv[0]: <to@example.com>
    rcpt_mailer: rcpt_mailer
    rcpt_host: rcpt_host
    rcpt_addr: <to@example.com>
mrb_xxfi_data
mrb_xxfi_header
    headerf: From
    headerv: <from@example.com>
mrb_xxfi_header
    headerf: To
    headerv: <to@example.com>
mrb_xxfi_header
    headerf: Subject
    headerv: Hello
mrb_xxfi_eoh
mrb_xxfi_body
    body: Hello world!!

mrb_xxfi_eom
    info->receive_time: 1477305542
    msg_id: (null)
mrb_xxfi_close
```
