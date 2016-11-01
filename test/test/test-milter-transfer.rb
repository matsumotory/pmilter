require "pathname"
require "milter/server"
require "milter/server/testing"
require "milter/client"
require "milter/client/testing"

require "milter-transfer"

class TestMilterTransfer < Test::Unit::TestCase

  SPEC = "unix:../hoge.sock"

  def setup
    @server = ::Milter::TestServer.new
    @milter_runner = ::Milter::Client::Test::MilterRunner.new(milter_path("milter-transfer"))
    @milter_runner.run
  end

  def teardown
    @milter_runner.stop
  end

  def test_no_emergency_mail
    result = @server.run(["--connection-spec", SPEC],
                         ["--mail-file", fixture_path("no-emergency.eml")],
                         ["--envelope-from", "from@example.com"],
                         ["--envelope-recipient", "to@example.com"])

    puts result.to_hash
    assert_equal("pass", result.status)
    assert_recipients(["to@example.com"], result.envelope_recipients)
    assert_equal([["From", "<from@example.com>"], ["To", "<to@example.com>"], ["Subject", "Hello"], ["+ X-Pmilter", "Enable"]], result.headers)
  end

  private

  def assert_recipients(expected, actual)
    actual = actual.map{|address|
      ::Milter::Client::EnvelopeAddress.new(address).address_spec
    }
    assert_equal(expected, actual)
  end

  def top_dir
    Pathname(__FILE__).realpath.dirname.parent
  end

  def bin_dir
    top_dir + "bin"
  end

  def milter_path(command_name)
    (bin_dir + command_name).to_s
  end

  def fixture_path(fixture_name)
    (top_dir + "test/fixtures/" + fixture_name).to_s
  end
end
