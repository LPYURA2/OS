"""Live UART snapshots must not treat a partial trailing line as a receipt."""
def complete_lines(raw):
    return raw[:raw.rfind(b'\n')+1].splitlines()
