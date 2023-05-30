#include <SD.h>
#include <SPI.h>
struct Pixel {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
};

void setup() {
    Serial.begin(9600);

    if (!SD.begin(4)) {
        Serial.println("Error al inicializar la tarjeta SD.");
        while (1);
    }

    String nombreArchivo = "/testing.bmp";
    File archivo = SD.open(nombreArchivo, FILE_READ);

    if (!archivo) {
        Serial.println("No se pudo abrir el archivo BMP.");
        while (1);
    }

    // Saltar la cabecera del archivo BMP
    archivo.seek(54);

    // Leer la información de la imagen
    uint32_t ancho;
    uint32_t alto;
    archivo.read(reinterpret_cast<uint8_t*>(&ancho), 4);
    archivo.read(reinterpret_cast<uint8_t*>(&alto), 4);

    // Calcular el tamaño del buffer de píxeles
    uint32_t tamanoBuffer = ancho * alto;
    Pixel buffer[tamanoBuffer];

    // Leer los píxeles de la imagen
    archivo.read(reinterpret_cast<uint8_t*>(buffer), tamanoBuffer * sizeof(Pixel));

    archivo.close();

    // Mostrar los píxeles en el buffer
    for (uint32_t i = 0; i < tamanoBuffer; i++) {
        Serial.print("R: ");
        Serial.print(buffer[i].red);
        Serial.print(" G: ");
        Serial.print(buffer[i].green);
        Serial.print(" B: ");
        Serial.println(buffer[i].blue);
    }
}

void loop() {
    // Tu código adicional aquí
}
