import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit

# Экспериментальные данные
theta_deg = np.array([0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])
N = np.array([896, 788, 806, 765, 705, 602, 532, 474, 437, 390, 356, 325, 314])
sigma_N = 0.01 * N                     # 1% погрешность канала

theta_rad = np.deg2rad(theta_deg)
x = 1 - np.cos(theta_rad)
y = 1 / N
sigma_x = np.sin(theta_rad) * np.deg2rad(2)   # 2° погрешность угла
sigma_y = sigma_N / N**2                      # = 0.01 * y

# Линейная аппроксимация y = a + b*x
def linear(x, a, b):
    return a + b * x

popt, pcov = curve_fit(linear, x, y, sigma=sigma_y, absolute_sigma=True)
a, b = popt
sigma_a, sigma_b = np.sqrt(np.diag(pcov))

# Вывод результатов
print(f"a = {a:.6e} ± {sigma_a:.2e}")
print(f"b = {b:.6e} ± {sigma_b:.2e}")

N0 = 1 / a
N90 = 1 / (a + b)
sigma_N0 = sigma_a / a**2
sigma_N90 = np.sqrt(sigma_a**2 + sigma_b**2) / (a + b)**2
print(f"N(0) = {N0:.1f} ± {sigma_N0:.1f}")
print(f"N(90) = {N90:.1f} ± {sigma_N90:.1f}")

E_gamma = 662  # кэВ
mc2 = E_gamma * N90 / (N0 - N90)
sigma_mc2 = mc2 * np.sqrt((sigma_a/a)**2 + (sigma_b/b)**2)
print(f"mc^2 = {mc2:.0f} ± {sigma_mc2:.0f} кэВ")

# Построение графика
plt.figure(figsize=(8, 6))
plt.errorbar(x, y, xerr=sigma_x, yerr=sigma_y, fmt='o', capsize=3)
x_fit = np.linspace(0, 1.5, 100)
plt.plot(x_fit, linear(x_fit, a, b), 'r-')
plt.xlabel('$1 - \cos\\theta$')
plt.ylabel('$1/N$')
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig('graph.pdf')
plt.show()
