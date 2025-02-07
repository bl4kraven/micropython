try:
    import ubus
except ImportError:
    print("SKIP")
    raise SystemExit

ubus.connect()
board_info, = ubus.call("system", "board", {}, 1000)
print(board_info["release"]["distribution"])
ubus.disconnect()