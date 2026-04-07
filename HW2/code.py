import board
import pwmio
import analogio
import time

# Set up PWM on GP16 for servo
# 50Hz frequency, 65535 max duty cycle
servo_pwm = pwmio.PWMOut(board.GP16, frequency=50, duty_cycle=0)

# Set up ADC on GP26
adc = analogio.AnalogIn(board.GP26)

def set_servo(angle):
    # Map angle (0-180) to duty cycle
    # 0.05 to 0.10 of the period (1ms to 2ms pulse at 50Hz)
    duty = 0.05 + (angle / 180.0) * 0.05
    servo_pwm.duty_cycle = int(duty * 65535)

while True:
    # Print voltage
    voltage = adc.value / 65535 * 3.3
    print(f"{voltage:.6f}")

    # Sweep up
    for i in range(10, 170):
        set_servo(i)
        time.sleep(0.01)

    # Sweep down
    for i in range(170, 10, -1):
        set_servo(i)
        time.sleep(0.01)