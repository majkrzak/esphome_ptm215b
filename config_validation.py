from esphome.config_validation import string_strict, Invalid


def security_key(value):
    value = string_strict(value)
    parts = value.split(":")
    if len(parts) != 16:
        raise Invalid("Security Key must consist of 16 : (colon) separated parts")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise Invalid(
            "Security Key must be format XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX"
        )
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError as exception:
            raise Invalid(
                "Security Key parts must be hexadecimal values from 00 to FF"
            ) from exception
    return parts_int
