import sys
import _pynx

PI = 3.14159265358979323846
TWO_PI = 2 * PI


def _sin_taylor(x):
    # Taylor series for sin, x assumed already reduced to [-pi, pi]
    x2 = x * x
    term = x
    total = x
    # a few terms give plenty of precision for |x| <= pi
    for n in range(1, 10):
        term *= -x2 / ((2 * n) * (2 * n + 1))
        total += term
    return total


def my_sin(x):
    # reduce x into [-pi, pi]
    x = x % TWO_PI
    if x > PI:
        x -= TWO_PI
    return _sin_taylor(x)


def my_cos(x):
    return my_sin(x + PI / 2)


def render_frame(A, B):
    # 80x22 resolution for Switch console
    output = [' '] * (80 * 22)
    zbuffer = [0] * (80 * 22)

    cos_A, sin_A = my_cos(A), my_sin(A)
    cos_B, sin_B = my_cos(B), my_sin(B)

    # theta: donut cross-section
    for j in range(0, 628, 7):
        theta = j / 100
        cos_theta, sin_theta = my_cos(theta), my_sin(theta)

        # phi: donut center rotation
        for i in range(0, 628, 2):
            phi = i / 100
            cos_phi, sin_phi = my_cos(phi), my_sin(phi)

            circle_x = cos_theta + 2
            circle_y = sin_theta

            x = circle_x * (cos_B * cos_phi + sin_A * sin_B * sin_phi) - circle_y * cos_A * sin_B
            y = circle_x * (sin_B * cos_phi - sin_A * cos_B * sin_phi) + circle_y * cos_A * cos_B
            z = 5 + cos_A * circle_x * sin_phi + circle_y * sin_A
            ooz = 1 / z

            K1 = 30
            xp = int(40 + K1 * ooz * x)
            yp = int(12 - K1 * ooz * y * 0.5)

            if 0 <= xp < 80 and 0 <= yp < 22:
                idx = xp + 80 * yp
                luminance = (cos_phi * cos_theta * sin_B
                             - cos_A * cos_theta * sin_phi
                             - sin_A * sin_theta
                             + cos_B * (cos_A * sin_theta - cos_theta * sin_A * sin_phi))

                if ooz > zbuffer[idx] and luminance > 0:
                    zbuffer[idx] = ooz
                    luminance_index = int(luminance * 8)
                    output[idx] = ".,-~:;=!*#$@"[max(0, min(11, luminance_index))]

    return output


def main():
    A, B = 1.0, 1.0
    print("Initializing Donut...")
    try:
        while True:
            frame = render_frame(A, B)

            # Clear screen and cursor to top-left
            sys.stdout.write('\033[H')

            output_str = []
            for row in range(22):
                output_str.append(''.join(frame[row * 80:(row + 1) * 80]))

            print('\n'.join(output_str))
            print("\nDon't forget to pay respects to uncle Sonic! Sony just doesn't get it.")
            print("Python version:", sys.version)

            A += 0.08
            B += 0.03

            # Flush and push framebuffer to screen
            _pynx.flush_console()
            _pynx.console_update()

            # Check if Plus button was pressed
            if _pynx.should_quit():
                break

            # Sleep using svcSleepThread (time.sleep can crash on Switch)
            _pynx.sleep_ms(33)
    except KeyboardInterrupt:
        print("\nBye")


if __name__ == "__main__":
    main()
