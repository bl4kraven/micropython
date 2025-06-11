try:
    import uci
except ImportError:
    print("SKIP")
    raise SystemExit

ret = uci.get_package("system")
print(ret[1])

try:
    uci.get_section("syste")
except ValueError:
    print("ValueError")

ret = uci.get_section("system.@system[0]")
print(ret["log_size"])


try:
    uci.get_section("system.@system[22]")
except ValueError:
    print("ValueError")

print(uci.get_value("system.@system[0].log_size"))

try:
    uci.get_value("system.@system[0].log_sssize")
except ValueError:
    print("ValueError")

try:
    uci.set_value("system.@system[0].test_mp", 1)
except ValueError:
    print("ValueError")

uci.set_value("system.@system[0].test_mp", "1")
print(uci.get_value("system.@system[0].test_mp"))

try:
    uci.del_key("system.@system[0].test_sssmp")
except ValueError:
    print("ValueError")

uci.del_key("system.@system[0].test_mp")
uci.commit_value("system")

try:
    uci.get_value("system.@system[0].test_mp")
except ValueError:
    print("ValueError")

uci.clear_buffer()
uci.free()