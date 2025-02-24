import tkinter as tk
from tkinter import ttk
import serial

# Configuración de la conexión serial
ser = serial.Serial('/dev/ttyUSB1', 9600)  # Asegúrate de ajustar el puerto COM según tu configuración

class LEDInterface:
    def __init__(self, root):
        self.root = root
        self.root.title("Control de Tira LED")

        # Frame para la selección de programas
        self.program_frame = ttk.LabelFrame(self.root, text="Seleccionar Programa")
        self.program_frame.grid(row=0, column=0, padx=10, pady=10)

        self.program_var = tk.StringVar()
        self.program_combobox = ttk.Combobox(self.program_frame, textvariable=self.program_var)
        self.program_combobox['values'] = ("Programa 1", "Programa 2", "Programa 3")
        self.program_combobox.grid(row=0, column=0, padx=10, pady=10)
        self.program_combobox.current(0)

        self.program_button = ttk.Button(self.program_frame, text="Iniciar", command=self.start_program)
        self.program_button.grid(row=0, column=1, padx=10, pady=10)

        # Frame para la selección de rondas
        self.round_frame = ttk.LabelFrame(self.root, text="Ajustar Rondas")
        self.round_frame.grid(row=1, column=0, padx=10, pady=10)

        self.round_var = tk.IntVar(value=1)
        self.round_label = ttk.Label(self.round_frame, text="Número de Rondas:")
        self.round_label.grid(row=0, column=0, padx=10, pady=10)
        self.round_spinbox = ttk.Spinbox(self.round_frame, from_=1, to=10, textvariable=self.round_var)
        self.round_spinbox.grid(row=0, column=1, padx=10, pady=10)

        self.round_button = ttk.Button(self.round_frame, text="Establecer", command=self.set_rounds)
        self.round_button.grid(row=0, column=2, padx=10, pady=10)

        # Frame para enviar colores a la tira
        self.color_frame = ttk.LabelFrame(self.root, text="Enviar Colores")
        self.color_frame.grid(row=2, column=0, padx=10, pady=10)

        self.color_var = tk.StringVar()
        self.color_combobox = ttk.Combobox(self.color_frame, textvariable=self.color_var)
        self.color_combobox['values'] = ("Rojo", "Verde", "Azul", "Off")
        self.color_combobox.grid(row=0, column=0, padx=10, pady=10)
        self.color_combobox.current(0)

        self.color_button = ttk.Button(self.color_frame, text="Enviar", command=self.send_color)
        self.color_button.grid(row=0, column=1, padx=10, pady=10)

    def start_program(self):
        program = self.program_var.get()
        print(f"Iniciando {program}")
        ser.write(f"START_{program}".encode())

    def set_rounds(self):
        rounds = self.round_var.get()
        print(f"Ajustando a {rounds} rondas")
        ser.write(f"ROUNDS_{rounds}".encode())

    def send_color(self):
        color = self.color_var.get()
        print(f"Enviando color {color}")
        ser.write(f"COLOR_{color}".encode())

if __name__ == "__main__":
    root = tk.Tk()
    app = LEDInterface(root)
    root.mainloop()
