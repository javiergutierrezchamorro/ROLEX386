/*
 * ROLEX386.C -- Reloj de pantalla para MS-DOS, al estilo del ROLEX.COM
 *               de RIX SoftWorks (1986).  Version FOTOREALISTA de 32
 *               bits para VGA 640x400x256 (VBE 2.0 + framebuffer
 *               lineal; el modo VBE 0x100), con la libreria vga256.
 *
 * C para Open Watcom v2 (386, DOS extendido, modelo flat).  Version
 * NATIVA de 640x480 (el CAMPO de la foto): la foto ya no es la escena
 * de 320x200 reescalada
 * 2x en marcha, sino que IMGRLE.H se genera DIRECTAMENTE a 640x480
 * desde las fotos originales (reloj2.png, esfera2.jpg) por promedio de
 * area, de modo que la nitidez es la de las fotos y no se reescala
 * nada por el camino.  NINGUNA geometria esta duplicada a mano: todo
 * se deriva por macros de IMGRLE.H, que sigue siendo la unica fuente
 * de la foto y de la calibracion:
 *     franja 494x400 en (88, 40): a los lados solo hay negro puro,
 *                                 que no viaja por el RLE
 *     dial 340x340 en (152, 70), con el centro en (322, 240) y el
 *                                 borde blanco en r=159.5: circulo
 *                                 PERFECTO sobre pixeles cuadrados
 *     agujas, remache y ventanita de la fecha: en sus coords finales
 *     VEN_* viene medido sobre la foto: el marco de la ventanita de
 *     las 3h queda en x 415..446 / y 230..250 y el dia se pinta justo
 *     dentro, en x 417..444 / y 232..248
 * El fondo RLE se decodifica UNA VEZ y DIRECTO a la VRAM lineal: ya
 * viaja a tamano final, asi que no hay paso por RAM, no hace falta
 * VGA256ScaleImage2x (aqui sobra) y se ahorran los 197.600 bytes de
 * la tira intermedia; la esfera limpia se recorta despues de la
 * propia VRAM, como en el original.
 *
 * La PANTALLA pasa a 640x400 CON EL MISMO IMGRLE.H: el campo sigue
 * siendo 640x480, CAM_Y sale -40 y la escena sube 40 pixeles, de modo
 * que la franja de 400 de alto llena la pantalla exacta (los margenes
 * de negro de arriba y abajo del campo nunca viajaron por el RLE, asi
 * que no se pierde nada).  Ojo al aspecto: en un monitor 4:3 el
 * 640x400 estira la imagen un 20% en vertical (pixeles 1:1,2, como el
 * 320x200 del ROLEX.COM original); si la esfera se ve algo ovalada, se
 * ajusta la altura del monitor o se compensa en prep_img.py.
 *
 * Se conservan las ganancias ALGORITMICAS del original, que en un 386
 * valen mucho mas que cualquier micro-optimizacion:
 *   - SOLO SE REDIBUJA CUANDO CAMBIA UN ANGULO: el angulo va en 1/256
 *     de vuelta y sale de los tics del BIOS (18.2 por segundo); el
 *     resto del tiempo el bucle solo mira tics y teclado.
 *   - FRANJAS SUCIAS: cada primitiva anota por fila el tramo de x que
 *     pinta; el fotograma siguiente restaura del marco solo esos
 *     tramos (copiando de la esfera limpia) y a la VRAM solo se vuelca
 *     la union de las franjas viejas y nuevas.
 *   - La fecha se "cocina" UNA VEZ en la esfera limpia (y se recocina
 *     al cruzar medianoche, marcando toda la esfera como sucia).
 *   - LUPA de la fecha (el "cyclops" de la ventanita de las 3h): una
 *     lente de 36x25 pixeles que AUMENTA 5/2 (2,5x, como la de Rolex)
 *     lo que hay debajo.  Bajo ella la esfera lleva la fecha a escala 1
 *     (digitos 5x7) y la lente la agranda por muestreo con dos tablas
 *     de indices (un acceso por pixel, sin multiplicar).  Esa imagen
 *     ampliada se cocina tambien en la esfera limpia, asi que sin agujas
 *     cerca no cuesta NADA por fotograma.  Cuando el minutero o el
 *     segundero pasan por la lente se recompone solo ese rectangulo: se
 *     restaura desde la esfera y encima de la fecha se pintan las
 *     agujas ensanchadas 5/2 (vectorial, recortadas al interior de la
 *     lente y con el mismo eje que fuera), con el aro por encima.
 *   - ANTI-PARPADEO: el fotograma se compone en RAM y solo se vuelca
 *     tras el retrazo vertical (VGA256WaitVRetrace).
 * Se sueltan en cambio las micro-optimizaciones de 8086 (seno y raiz
 * cuadrada incrementales): a 18 fps el dibujo de las agujas (unos
 * pocos miles de ciclos) no se nota.  Lo que SI cuenta en un 386 es el
 * volcado a memoria: las franjas se copian alineadas a dword (rep movsd
 * puro, sin colas de bytes) y los bucles por filas avanzan con punteros
 * en vez de calcular y * ancho (el IMUL del 386 tarda 9..38 ciclos y un
 * desplazamiento 3).  Las franjas sucias y el retrazo se quedan porque
 * son ganancias de algoritmo, no de ciclos.
 *
 * Hardware, TODO por la vga256: VBE_Init / VBE_FindMode / VBE_SetMode
 * (lineal) + VBE_GetVideoPtr para el LFB, VGA256SetPalette (DAC de 6
 * bits, valores /4 como en el original) y VGA256WaitVRetrace.  Bajo
 * DPMI el primer megabyte esta mapeado 1:1 (la propia vga256 usa la
 * fuente del BIOS en 0xFFA6E), de modo que el contador de tics del
 * BIOS se sigue leyendo de 0x46C; la cola del teclado del BIOS
 * tambien (0040:001A/001C == 0x41A/0x41C) y la VRAM de texto del
 * modo 3 (0xB8000), que usa la franja de salida.  El dia del mes sale
 * del DOS (int 21h AH=2Ah).  El teclado se sondea en el BDA (dos
 * lecturas de 16 bits) y solo se baja a la int 16h cuando hay tecla:
 * kbhit costaba una transicion al modo real del DPMI POR SONDEO; ahora
 * ademas solo se sondea al cambiar el tic (como mucho 55 ms de latencia).
 * Al salir (ESC) no se llama al DOS: el copyright se pinta DIRECTO en
 * la VRAM de texto (franja azul de 80 columnas en la fila 0, texto
 * blanco, cursor en (1, 0)): ni cputs ni int 29h.
 *
 * Tic-tac: si hay un YM3812 (Adlib o Sound Blaster) en 388h, cuatro
 * veces por segundo sale un click FM (el tic algo mas agudo que el
 * tac); si no hay chip, FALLBACK al altavoz del PC (canal 2 del PIT
 * 8253 al puerto 42h y bits 0/1 del 61h, ~55 ms de tono por tic del
 * BIOS), como en ROLEX.C.  Nada de quedarse mudo.
 *
 * El segundero pinta DOS pixeles de ancho: sobre los pixeles finos de
 * 640x480, el hilo de un pixel quedaria casi invisible.  El resto de
 * las agujas (baston hexagonal con contorno oscuro y facetas
 * brillante/clara) es el de siempre, con cada capa encogida DOS
 * pixeles por lado.
 *
 * Compilar (Open Watcom v2, 32 bits, DOS):
 *      wcl386 -bt=dos -ox -ot -d0 -dVGA256_MODE_640X400X8 ROLEX386.C vga256.c -> ROLEX386.EXE
 * (ROLEX386.C y vga256.c deben ver la misma VGA256_MODE_*: la linea
 *  de arriba define VGA256_MODE_640X400X8 para los DOS modulos y ambos
 *  pasan a 640x400x8, un modo nuevo de vga256.h.  El EXE necesita un
 *  DPMI: el DOS/4GW de siempre, que en DOSBox, QEMU o 86Box funciona
 *  sin tocar nada.)
 *
 * Regenerar la imagen desde las fotos (reloj2.png, esfera2.jpg):
 *      python3 prep_img.py reloj2.png esfera2.jpg IMGRLE.H
 *
 * Mejoras v2 (C puro, sin ASM ni pragma aux):
 *   - Tabla SIN256[] de 320 entradas (lookup O(1)): el coseno es la
 *     misma tabla desplazada 64 (cos a = SIN256[a + 64]), sin mascaras.
 *   - Tabla fila340[] de offsets de fila del dial (sin IMUL por fila).
 *   - Franjas sucias con margen 1 px.
 *   - Speaker apagado por delta de tics.
 *   - VGA256WaitVRetrace ya tiene timeout en vga256.c mejorado.
 */


#include <string.h>   /* memcpy, memset                 */
#include <conio.h>    /* cputs (errores), inp/outp      */
#include <dos.h>      /* union REGS, int386             */


#ifndef VGA256_MODE_640X400X8
#define VGA256_MODE_640X400X8
#endif

#include "vga256.h"   /* VBE + primitivas VGA            */
#include "ROLEX386.H"   /* foto, paleta y geometria (nativa 640x480) */

/* COMPROBACION DE MODO, en compilacion y sin coste: la pantalla tiene
   que ser 640x400.  Si el -dVGA256_MODE_640X400X8 no llega o si vga256.h
   es la vieja (la que no conoce el modo), la cabecera caeria en
   640x480x8 SIN DECIR NADA y el reloj saldria con las bandas negras de
   40 pixeles arriba y abajo del 640x480, que es justo lo que se ve en
   una captura con margenes.  Con esto, mal compilado = error claro. */
#if VGA256_HEIGHT != 400
#error Compila con -dVGA256_MODE_640X400X8 (y con la vga256.h nueva, que conoce el 640x400)
#endif

/* ==================== geometria de la escena ==================== */

/* La escena es nativa de 640x480 (el CAMPO) e IMGRLE.H la da en coords
   de campo: franja RLE 494x400 en (STRIP_X, STRIP_Y) = (88, 40) y
   dial 340x340 en (DIAL_X, DIAL_Y) = (152, 70).  En 640x400 CAM_Y sale
   NEGATIVO (-40): el campo sube 40 pixeles, la franja de 400 de alto
   llena la pantalla de arriba a abajo y no se pierde nada (los
   margenes del campo son negro que no viaja por el RLE).  En un modo
   mayor (800x600, 1024x768) el campo quedaria centrado. */
#define CAMPO_W  640                     /* ancho del campo 640x480      */
#define CAMPO_H  480                     /* alto  del campo 640x480      */
#define CAM_X    ((VGA256_WIDTH  - CAMPO_W) / 2)   /* 0 en 640x480       */
#define CAM_Y    ((VGA256_HEIGHT - CAMPO_H) / 2)   /* 0 en 640x480       */
#define TIRA_H   400                     /* alto de la franja RLE        */
#define TIRA_X   (CAM_X + STRIP_X)       /* franja en pantalla           */
#define TIRA_Y   (CAM_Y + STRIP_Y)
#define DIAL_XF  (CAM_X + DIAL_X)        /* dial en pantalla             */
#define DIAL_YF  (CAM_Y + DIAL_Y)
#define DIAL_TAM ((unsigned)DIAL_W * DIAL_H)   /* constante: 115.600     */
#define DCX      (DIAL_W / 2)            /* centro del dial en local     */
#define DCY      (DIAL_H / 2)

/* ventanita de la fecha, en coords del dial (IMGRLE.H la da en coords
   de pantalla 640x480, medida sobre la foto) */
#define VEN_XL    (VEN_X0 - DIAL_X)
#define VEN_YL    (VEN_Y0 - DIAL_Y)
#define VEN_ANCHO (VEN_X1 - VEN_X0 + 1)
#define VEN_ALTO  (VEN_Y1 - VEN_Y0 + 1)

/* digitos de la fuente: 5x7, pintados a 2x (10x14 cada uno) */
#define DIG_W    5
#define DIG_ALTO 7

/* LUPA de la fecha (coords del dial).  La lente es un rectangulo
   LUP_W x LUP_H centrado en la ventanita que AUMENTA 5/2: solo ve un
   trozo de LUP_SW x LUP_SH pixeles de la esfera (el 2/5 central,
   redondeado hacia arriba), justo el tamano de dos digitos 5x7 a
   escala 1.  LUP_R* es el rectangulo con el aro de cristal (un pixel
   por fuera de la lente). */
#define LUP_W    36
#define LUP_H    25
#define LUP_X0   (VEN_XL + (VEN_ANCHO - LUP_W) / 2)
#define LUP_Y0   (VEN_YL + (VEN_ALTO - LUP_H) / 2)
#define LUP_RX0  (LUP_X0 - 1)
#define LUP_RY0  (LUP_Y0 - 1)
#define LUP_RW   (LUP_W + 2)
#define LUP_RH   (LUP_H + 2)
#define LUP_RX1  (LUP_RX0 + LUP_RW - 1)
#define LUP_RY1  (LUP_RY0 + LUP_RH - 1)
#define LUP_SW   15                          /* (LUP_W * 2 + 4) / 5   */
#define LUP_SH   10                          /* LUP_H * 2 / 5         */
#define LUP_SX0  (LUP_X0 + LUP_W / 2 - LUP_SW / 2)
#define LUP_SY0  (LUP_Y0 + LUP_H / 2 - LUP_SH / 2)

/* hasta donde llega, fuera de la lente, una aguja cuya imagen AMPLIADA
   toca la lente: el semiancho de la aguja mas ancha ampliada (15) por
   el peor caso en diagonal (1,42) */
#define LUP_M    24

/* digitos de la fecha a escala 1 dentro de la fuente de la lente */
#define FEC_X1   ((LUP_SW - DIG_W + 1) / 2)              /* un digito   */
#define FEC_X2   ((LUP_SW - (2 * DIG_W + 1) + 1) / 2)    /* dos digitos */
#define FEC_Y    ((LUP_SH - DIG_ALTO + 1) / 2)

/* indices fijos de la paleta (los mismos que fija prep_img.py) */
#define C_OSC 250           /* acero oscuro (bordes de las agujas)   */
#define C_CLA 252           /* acero claro (cuerpo)                   */
#define C_HI  253           /* acero brillo (faceta central)          */
#define C_NEG 0             /* negro (digitos de la fecha)            */
#define C_ARO1 46           /* aro de la lupa: arriba e izquierda     */
#define C_ARO2 34           /* aro de la lupa: abajo y derecha        */

/* comprobaciones de la cabecera generada (en compilacion, sin coste):
   la franja cabe en el campo, el segundero se dibuja sin recortar (y
   pinta DOS pixeles de ancho, de ahi el +2) y la lupa cabe en el dial,
   tapa la ventanita y ve una fuente que contiene los dos digitos */
#if (STRIP_X + STRIP_W > CAMPO_W) || (STRIP_Y + TIRA_H > CAMPO_H)
#error La franja RLE no cabe en el campo 640x480
#endif
#if (R_AG_S + 2 >= DCX) || (COL_S + 2 >= DCX)
#error El segundero se sale del dial: linea() no recorta
#endif
#if (DIAL_W & 3) || (DIAL_XF & 3) || (VGA256_WIDTH & 3)
#error Las franjas se copian alineadas a dword: DIAL_W, DIAL_XF y el ancho de pantalla deben ser multiplos de 4
#endif
#if (LUP_RX0 < 0) || (LUP_RX1 >= DIAL_W) || (LUP_RY0 < 0) || (LUP_RY1 >= DIAL_H)
#error La lupa de la fecha no cabe en el dial
#endif
#if (LUP_W < VEN_ANCHO + 4) || (LUP_H < VEN_ALTO + 4)
#error La lupa no tapa la ventanita de la fecha
#endif
#if ((2 * (LUP_W - 1) + 1) / 5 >= LUP_SW) || ((2 * (LUP_H - 1) + 1) / 5 >= LUP_SH)
#error LUP_SW x LUP_SH no cubre lo que ve la lente (aumento 5/2)
#endif
#if (LUP_SW < 2 * DIG_W + 1) || (LUP_SH < DIG_ALTO)
#error La fuente de la lente no cabe para dos digitos
#endif

/* ============================ datos ============================ */

/* buferes de trabajo: marco + esfera limpia (2 x 115.600) en un unico
   bloque estatico, alineado a dword por ser de unsigned long (DIAL_TAM
   es multiplo de 4): nada de malloc ni de ruta de error.  MARCO y
   ESFERA son macros (direcciones constantes) y no punteros: el codigo
   no tiene que cargar una variable en cada uso */
static unsigned long almacen[(2 * DIAL_TAM) / 4];
#define MARCO  ((unsigned char *)almacen)    /* fotograma compuesto (340x340) */
#define ESFERA (MARCO + DIAL_TAM)            /* esfera limpia (base)          */
static unsigned char *pVideo;  /* framebuffer lineal de la vga256   */

/* Tabla de seno Q8: SEN[k] = round(sin(k*2*pi/256)*256), k = 0..64;
   el resto por simetria (init_sincos). */
static const int SEN[65] = {
      0,    6,   13,   19,   25,   31,   38,   44,
     50,   56,   62,   68,   74,   80,   86,   92,
     98,  104,  109,  115,  121,  126,  132,  137,
    142,  147,  152,  157,  162,  167,  172,  177,
    181,  185,  190,  194,  198,  202,  206,  209,
    213,  216,  220,  223,  226,  229,  231,  234,
    237,  239,  241,  243,  245,  247,  248,  250,
    251,  252,  253,  254,  255,  255,  256,  256,  256
};

/* Tabla de 256 + 64 entradas: SIN256[a] es el seno y SIN256[a + 64] el
   coseno (Q8) del angulo a, en 1/256 de vuelta (a < 256, sin mascaras ni
   segunda tabla); y offsets de fila del dial (sin IMUL). */
static int SIN256[256 + 64];
static unsigned fila340[DIAL_H];

static void init_sincos(void)
{
    int k;
    unsigned o = 0;

    for (k = 0; k <= 64; k++) {          /* el resto, por simetria */
        SIN256[k]       =  SEN[k];
        SIN256[128 - k] =  SEN[k];
        SIN256[128 + k] = -SEN[k];
        SIN256[256 - k] = -SEN[k];
    }
    for (k = 256; k < 256 + 64; k++)     /* cos(a) = sen(a + 64) */
        SIN256[k] = SIN256[k - 256];
    for (k = 0; k < DIAL_H; k++, o += DIAL_W)
        fila340[k] = o;
}

#define OFF340(y) (fila340[(y)])

/* Digitos 5x7 para la fecha.  Bit 0x80 = pixel izquierdo. */
static const unsigned char FUENTE[10][DIG_ALTO] = {
    { 0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70 },  /* 0 */
    { 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70 },  /* 1 */
    { 0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xF8 },  /* 2 */
    { 0xF0, 0x08, 0x08, 0x70, 0x08, 0x08, 0xF0 },  /* 3 */
    { 0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10 },  /* 4 */
    { 0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70 },  /* 5 */
    { 0x30, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70 },  /* 6 */
    { 0xF8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40 },  /* 7 */
    { 0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70 },  /* 8 */
    { 0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60 }   /* 9 */
};

/* tablas de cruces del relleno de poligonos: se reponen POR BLOQUE con
   memset (0x7F repetido da un entero muy positivo y 0x80 uno muy
   negativo: centinelas validos con int de 16 y de 32 bits) */
static int cruce_min[DIAL_H];
static int cruce_max[DIAL_H];

/* ==================== primitivas de dibujo ==================== */

/* franjas sucias: [x min, x max] pintado en cada fila por las
   primitivas, en dos juegos (fotograma actual y anterior; act es el del
   ultimo fotograma compuesto).  Vacio = min muy positivo y max muy
   negativo.  cmin/cmax apuntan al juego en el que se anota. */
static int suc_min[2][DIAL_H];
static int suc_max[2][DIAL_H];
static int act;
static int *cmin = suc_min[0];
static int *cmax = suc_max[0];

/* margen 1 px contra rastros en diagonales */
static void sucia(int x1, int x2, int y)
{
    if (x1 > 0) x1--;
    if (x2 < DIAL_W - 1) x2++;
    if (x1 < cmin[y]) cmin[y] = x1;
    if (x2 > cmax[y]) cmax[y] = x2;
}

/* toda la esfera sucia: tras cocinar la fecha, para que el marco y la
   VRAM recojan la esfera entera en el siguiente fotograma */
static void sucio_todo(void)
{
    int y;
    for (y = 0; y < DIAL_H; y++) {
        suc_min[act][y] = 0;
        suc_max[act][y] = DIAL_W - 1;
    }
}

static int clip_x0 = 0, clip_x1 = DIAL_W - 1;   /* ventana de recorte: todo el dial */
static int clip_y0 = 0, clip_y1 = DIAL_H - 1;   /* (la lupa la estrecha a su interior) */

static void linea_h(int x1, int x2, int y, unsigned char c)
{
    int t;
    if (y < clip_y0 || y > clip_y1) return;
    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (x1 < clip_x0) x1 = clip_x0;
    if (x2 > clip_x1) x2 = clip_x1;
    if (x1 > x2) return;
    sucia(x1, x2, y);
    memset(MARCO + OFF340(y) + (unsigned)x1, c, (size_t)(x2 - x1 + 1));
}

/* poligono convexo relleno por barrido de scanlines, recortado al
   dial.  Cada arista se recorre con error de Bresenham por filas (solo
   sumas y comparaciones), con el error inicial a media pendiente para
   que x salga REDONDEADA y no truncada, y las filas de los dos extremos
   incluidas.  Los cruces de cada fila se apuntan en las tablas, que se
   reponen con memset y se pintan al final con linea_h. */
static void poligono(int n, const int *xs, const int *ys, unsigned char c)
{
    int i, y, ymin = DIAL_H - 1, ymax = 0;

    for (i = 0; i < n; i++) {
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }
    if (ymin < 0) ymin = 0;
    if (ymax > DIAL_H - 1) ymax = DIAL_H - 1;
    if (ymin > ymax) return;

    memset((char *)(cruce_min + ymin), 0x7F,
           (unsigned)(ymax - ymin + 1) * sizeof(int));
    memset((char *)(cruce_max + ymin), 0x80,
           (unsigned)(ymax - ymin + 1) * sizeof(int));

    for (i = 0; i < n; i++) {
        int j = (i + 1 == n) ? 0 : i + 1;
        int x, dx, dy, sx = 1, err, yfin;

        if (ys[i] == ys[j]) continue;
        if (ys[i] < ys[j]) { x = xs[i]; y = ys[i]; dx = xs[j] - x; dy = ys[j] - y; }
        else               { x = xs[j]; y = ys[j]; dx = xs[i] - x; dy = ys[i] - y; }
        if (dx < 0) { sx = -1; dx = -dx; }
        err = dy >> 1;
        yfin = y + dy + 1;
        if (yfin > ymax + 1) yfin = ymax + 1;
        for (; y < yfin; y++) {
            if (y >= ymin) {
                if (x < cruce_min[y]) cruce_min[y] = x;
                if (x > cruce_max[y]) cruce_max[y] = x;
            }
            err += dx;
            while (err >= dy) { err -= dy; x += sx; }
        }
    }
    for (y = ymin; y <= ymax; y++)
        if (cruce_max[y] >= cruce_min[y])
            linea_h(cruce_min[y], cruce_max[y], y, c);
}

/* disco solido (remache central): por fila, el semiancho es la mayor h
   con h*h <= r*r - dy*dy.  Con radios de 2 a 6, el MUL y la busqueda
   hacia abajo salen mas baratos (y mas claros) que las sumas
   incrementales de r*r y de la raiz del original */
static void disco(int cx, int cy, int r, unsigned char c)
{
    int r2 = r * r;
    int dy;

    for (dy = 0; dy <= r; dy++) {
        int limite = r2 - dy * dy;      /* queda r*r - dy*dy   */
        int h = r;
        while (h * h > limite) h--;
        linea_h(cx - h, cx + h, cy + dy, c);
        if (dy != 0)
            linea_h(cx - h, cx + h, cy - dy, c);
    }
}

/* punto del dial: s y c son seno y coseno (Q8) del angulo de la aguja,
   rr va a lo largo de la aguja (negativo = cola) y k es perpendicular.
   El +128 redondea al pixel mas cercano.  La Y de la pantalla crece
   hacia abajo, de ahi el signo menos del coseno. */
static void punto(int s, int c, int rr, int k, int *x, int *y)
{
    *x = DCX + ((s * rr + c * k + 128) >> 8);
    *y = DCY + ((s * k - c * rr + 128) >> 8);
}

/* linea de Bresenham (segundero), POR PUNTERO y SIN recorte (la
   comprobacion #if de arriba garantiza que cae dentro del dial),
   anotando la franja sucia UNA vez por fila.  Pinta DOS pixeles
   horizontales por punto: sobre los pixeles finos de 640x480, el hilo
   de un pixel quedaria casi invisible. */
static void linea(int x0, int y0, int x1, int y1, unsigned char c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx, sy, err, e2, xa, xr = x0, paso;
    unsigned char *p;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    paso = (sy > 0) ? DIAL_W : -DIAL_W;
    err = dx - dy;
    p = MARCO + OFF340(y0) + (unsigned)x0;
    for (;;) {
        p[0] = c;
        p[1] = c;
        if (x0 == x1 && y0 == y1) break;
        xa = x0;                       /* ultimo pixel pintado de la fila */
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 += sx; p += sx; }
        if (e2 <  dx) {
            err += dx;
            if (xr < xa) sucia(xr, xa + 1, y0); else sucia(xa, xr + 1, y0);
            y0 += sy; p += paso;
            xr = x0;                   /* empieza el tramo de la fila nueva */
        }
    }
    if (xr < x0) sucia(xr, x0 + 1, y0); else sucia(x0, xr + 1, y0);
}

/* proporciones de la aguja (valores TRUNCADOS, hw de 1 a 8; dos
   productos por aguja y fotograma, que no se notan):
     hwt   = 0.60 * hw  (77/128)     hwtip = 0.55 * hw  (141/256) */
#define HWT(hw)   (((hw) * 77)  >> 7)
#define HWTIP(hw) (((hw) * 141) >> 8)

/* baston: aguja hexagonal (punta plana, flancos anchos junto al
   centro, cola estrecha), como las de oro blanco del Datejust */
static void baston(int s, int c, int r1, int cola, int hw, unsigned char col)
{
    int rrm = r1 >> 2;               /* donde la aguja es mas ancha */
    int hwt = HWT(hw);               /* semiancho de la cola        */
    int hwtip = HWTIP(hw);           /* semiancho en la punta       */
    int x[6], y[6];
    punto(s, c, r1,  -hwtip, &x[0], &y[0]);
    punto(s, c, r1,   hwtip, &x[1], &y[1]);
    punto(s, c, rrm,  hw,   &x[2], &y[2]);
    punto(s, c, -cola,  hwt, &x[3], &y[3]);
    punto(s, c, -cola, -hwt, &x[4], &y[4]);
    punto(s, c, rrm, -hw,   &x[5], &y[5]);
    poligono(6, x, y, col);
}

/* baston con cuerpo: contorno oscuro + faceta brillante + faceta clara.
   Cada capa se encoge DOS pixeles por TODOS los lados (r1, cola y hw),
   que sobre los pixeles finos del 640x480 es lo que en el original de
   320x200 era encogerse uno. */
static void baston2(unsigned ang, int r1, int cola, int hw)
{
    int s = SIN256[ang], c = SIN256[ang + 64];
    baston(s, c, r1, cola, hw, C_OSC);
    if (hw >= 4) baston(s, c, r1 - 2, cola - 2, hw - 2, C_HI);
    if (hw >= 6) baston(s, c, r1 - 4, cola - 4, hw - 4, C_CLA);
}

/* segundero: hilo fino de cola a punta (la real es un hilo negro; sin
   contrapeso circular) */
static void segundero(unsigned ang)
{
    int s = SIN256[ang], c = SIN256[ang + 64];
    int xa, ya, xb, yb;
    punto(s, c, -COL_S, 0, &xa, &ya);
    punto(s, c,  R_AG_S, 0, &xb, &yb);
    linea(xa, ya, xb, yb, C_OSC);
}

/* remache central sobre el eje de las agujas */
static void centro(void)
{
    disco(DCX, DCY, CEN_RO, C_OSC);
    disco(DCX, DCY, CEN_RI, C_CLA);
    disco(DCX, DCY, CEN_PN, C_OSC);
}

/* ==================== lupa y fecha cocinadas en la esfera ==================== */

/* La fuente de la lente es el trozo de esfera de LUP_SW x LUP_SH pixeles
   que hay debajo de ella, a escala 1.  lup_foto guarda el fondo de la
   ventanita tal como sale de la foto (sin fecha) y lup_fuente le suma la
   fecha del dia.  lup_cx / lup_cy dicen de que columna / fila de la
   fuente sale cada columna / fila de la lente (lup_cy ya multiplicado
   por LUP_SW): el aumento 5/2 hace que cada pixel de la fuente salga 2
   o 3 veces, y con las tablas cada pixel de la lente es UN acceso. */
static unsigned char lup_foto[LUP_SW * LUP_SH];
static unsigned char lup_fuente[LUP_SW * LUP_SH];
static unsigned char lup_cx[LUP_W];
static unsigned char lup_cy[LUP_H];

/* destino i -> fuente (2i+1)/5: se toma el centro del pixel de la lente
   (i + 1/2) por 2/5; el numerador va de 2 en 2 y se lleva el cociente
   por sumas, sin dividir */
static void init_lupa(void)
{
    int i, q = 0, r = 1;
    unsigned o = 0;

    for (i = 0; i < LUP_W; i++) {
        lup_cx[i] = (unsigned char)q;
        r += 2;
        if (r >= 5) { r -= 5; q++; }
    }
    for (i = 0, r = 1; i < LUP_H; i++) {
        lup_cy[i] = (unsigned char)o;
        r += 2;
        if (r >= 5) { r -= 5; o += LUP_SW; }
    }
}

/* pinta el interior de la lente en el dial (ESFERA o MARCO) agrandando
   la fuente.  Las filas de la lente que salen de la misma fila de la
   fuente (la mitad) se copian de la anterior */
static void lente(unsigned char *dial, const unsigned char *fuente)
{
    unsigned char *d = dial + (LUP_Y0 * DIAL_W + LUP_X0);
    const unsigned char *s;
    int i, j;

    for (j = 0; j < LUP_H; j++, d += DIAL_W) {
        if (j != 0 && lup_cy[j] == lup_cy[j - 1]) {
            memcpy(d, d - DIAL_W, LUP_W);
            continue;
        }
        s = fuente + lup_cy[j];
        for (i = 0; i < LUP_W; i++)
            d[i] = s[lup_cx[i]];
    }
}

/* aro del cristal, un pixel por fuera de la lente (sin las esquinas, que
   quedan redondeadas): claro arriba y a la izquierda, oscuro abajo y a
   la derecha */
static void aro(unsigned char *dial)
{
    unsigned char *d = dial + (LUP_RY0 * DIAL_W + LUP_RX0);
    int j;

    memset(d + 1, C_ARO1, LUP_RW - 2);
    for (j = 1, d += DIAL_W; j < LUP_RH - 1; j++, d += DIAL_W) {
        d[0] = C_ARO1;
        d[LUP_RW - 1] = C_ARO2;
    }
    memset(d + 1, C_ARO2, LUP_RW - 2);
}

/* Puede alguna aguja del fotograma recien pintado verse bajo la lente?
   Las franjas sucias lo dicen sin mirar un solo pixel (con un margen:
   bajo el cristal la aguja se ve mas ancha que fuera) */
static int lupa_toca(void)
{
    int y, y0 = LUP_RY0 - LUP_M, y1 = LUP_RY1 + LUP_M;

    if (y0 < 0) y0 = 0;
    if (y1 > DIAL_H - 1) y1 = DIAL_H - 1;
    for (y = y0; y <= y1; y++)
        if (cmax[y] >= LUP_RX0 - LUP_M && cmin[y] <= LUP_RX1 + LUP_M)
            return 1;
    return 0;
}

/* recompone la lente con las agujas: se devuelve a su sitio todo el
   rectangulo (fecha ampliada y aro) desde la esfera, lo que borra las
   agujas pintadas SIN aumento encima de la lente, y encima de la fecha se
   vuelven a pintar el minutero y el segundero AMPLIADOS 5/2 en anchura
   (la misma aguja, con el semiancho por 5/2 y recortada al interior de
   la lente, de modo que su eje sigue en linea con la aguja de fuera).
   Es vectorial: sin escalones de muestreo.  El horario no llega nunca
   (R_AG_H queda a 3 pixeles de la lente) */
static void lupa(unsigned a_m, unsigned a_s)
{
    unsigned char *m = MARCO  + (LUP_RY0 * DIAL_W + LUP_RX0);
    const unsigned char *e = ESFERA + (LUP_RY0 * DIAL_W + LUP_RX0);
    int y, s = SIN256[a_m], c = SIN256[a_m + 64];

    for (y = LUP_RY0; y <= LUP_RY1; y++, m += DIAL_W, e += DIAL_W) {
        memcpy(m, e, LUP_RW);
        sucia(LUP_RX0, LUP_RX1, y);
    }
    clip_x0 = LUP_X0;  clip_x1 = LUP_X0 + LUP_W - 1;
    clip_y0 = LUP_Y0;  clip_y1 = LUP_Y0 + LUP_H - 1;
    baston(s, c, R_AG_M,     COL_M,     (HW_M * 5) >> 1,       C_OSC);
    baston(s, c, R_AG_M - 2, COL_M - 2, ((HW_M - 2) * 5) >> 1, C_HI);
    baston(s, c, R_AG_M - 4, COL_M - 4, ((HW_M - 4) * 5) >> 1, C_CLA);
    baston(SIN256[a_s], SIN256[a_s + 64], R_AG_S, COL_S, 4, C_OSC);
    clip_x0 = 0;  clip_x1 = DIAL_W - 1;
    clip_y0 = 0;  clip_y1 = DIAL_H - 1;
}

/* ==================== fecha cocinada en la esfera ==================== */

/* un digito 5x7 a escala 1 en la fuente de la lente (la lente se
   encarga de agrandarlo) */
static void digito(int x, int y, int v)
{
    const unsigned char *g = FUENTE[v];
    unsigned char *d = lup_fuente + y * LUP_SW + x;
    unsigned char m;
    int f, b;

    for (f = 0; f < DIG_ALTO; f++, d += LUP_SW)
        for (b = 0, m = 0x80; b < DIG_W; b++, m >>= 1)
            if (g[f] & m) d[b] = C_NEG;
}

/* dia del mes (sin cero inicial) bajo la lente, centrado.  Se cocina
   UNA VEZ sobre la esfera limpia y a medianoche se vuelve a cocinar con
   el dia nuevo. */
static void pinta_fecha(int dia)
{
    memcpy(lup_fuente, lup_foto, sizeof(lup_fuente));
    if (dia < 10) {                     /* un digito, centrado */
        digito(FEC_X1, FEC_Y, dia);
    } else {
        int dec = 0;
        while (dia >= 10) { dia -= 10; dec++; }   /* dia / 10 y dia % 10 */
        digito(FEC_X2,             FEC_Y, dec);
        digito(FEC_X2 + DIG_W + 1, FEC_Y, dia);
    }
    lente(ESFERA, lup_fuente);
    aro(ESFERA);
    sucio_todo();
}

/* ============================ hardware ============================ */

/* contador de tics del BIOS (32 bits) en 0040:006C == lineal 0x46C
   (el DPMI mapea el primer megabyte 1:1).  Esta alineado a dword, asi
   que UNA lectura de 32 bits es atomica frente a la IRQ0: no hace falta
   el truco de leer dos mitades y comprobar */
static unsigned long lee_ticks(void)
{
    return *(volatile unsigned long *)0x46Cul;
}

/* dia del mes del DOS (int 21h AH=2Ah; el DPMI refleja las llamadas DOS
   al modo real y el DOS ya lo lee del RTC y lo adelanta solo al cruzar
   medianoche) */
static int lee_fecha(void)
{
    union REGS r;
    r.h.ah = 0x2A;
    int386(0x21, &r, &r);
    return r.h.dl;
}

/* ==================== teclado: cola del BIOS en el BDA ==================== */

/* El bucle principal sondea la cola del teclado directamente en el BDA
   (0040:001A cabeza / 0040:001C cola == lineal 0x41A/0x41C por el
   mapeo 1:1 del primer megabyte): dos lecturas de 16 bits y nada mas.
   Antes se pagaba kbhit (una int 16h, con su transicion al modo real
   del DPMI) POR SONDEO, miles de veces por segundo.  Cuando HAY tecla
   se extrae con la int 16h AH=00h, que devuelve ASCII y deja al BIOS
   como dueno del teclado.  El puerto 60h no compensa: el IRQ1 del
   BIOS sigue leyendo el controlador hacia la misma cola (habria que
   quitarle el IRQ1 para no duplicar o perder teclas), hay que
   separar make de break y prefijos E0 y traducir el scancode, y no se
   gana nada sobre dos lecturas de memoria.  (Con la cola llena tambien
   cabeza==cola: el caso esquina de siempre, que el BIOS ya pita.) */
static int hay_tecla(void)
{
    volatile unsigned short *bda = (volatile unsigned short *)0x41Aul;
    return bda[0] != bda[1];              /* cabeza != cola */
}

static int lee_tecla(void)
{
    union REGS r;
    r.h.ah = 0x00;                        /* int 16h AH=00h: extraer */
    int386(0x16, &r, &r);
    return (int)(unsigned char)r.h.al;     /* ASCII (0 si extendida) */
}

/* ==================== tic-tac FM (Adlib / Sound Blaster) ==================== */

/* Un YM3812 (OPL2) responde en 388h/389h tanto en el Adlib como en
   cualquier Sound Blaster.  El tic-tac son dos clics alternos, un
   click por cuarto de segundo: un solo canal FM (el 0) preparado con
   ataque inmediato y caida y salida rapidisimas, de modo que cada
   key-on ES el click; el tic es algo mas agudo que el tac.  Sin chip,
   FALLBACK al altavoz del PC (pitido/callar de mas abajo). */
#define OPL_REG 0x388u
#define OPL_DAT 0x389u

/* escritura de registro con las esperas clasicas del Adlib, hechas
   leyendo el propio puerto (6 lecturas tras el registro, 35 tras el
   dato), como en los drivers de siempre */
static void opl_escribe(unsigned char reg, unsigned char val)
{
    int i;
    outp(OPL_REG, reg);
    for (i = 0; i < 6; i++)  (void)inp(OPL_REG);
    outp(OPL_DAT, val);
    for (i = 0; i < 35; i++) (void)inp(OPL_REG);
}

/* deteccion clasica: apretar los timers del YM3812 y mirar el
   registro de estado: limpio (0) tras el reset y, tras arrancar el
   timer 1 y esperar sus 80 us, con el bit 7 (IRQ) y el 6 (timer 1) a 1,
   o sea 0xC0.  (Antes se comparaba con 0x40 y sin esperar: el chip
   NUNCA se detectaba y el reloj quedaba siempre mudo.) */
static int opl_presente(void)
{
    int s1, s2, i;
    opl_escribe(0x04, 0x60);              /* reset de los timers     */
    opl_escribe(0x04, 0x80);
    s1 = inp(OPL_REG) & 0xE0;
    opl_escribe(0x02, 0xFF);              /* timer 1 a tope, marcha  */
    opl_escribe(0x04, 0x21);
    for (i = 0; i < 200; i++) (void)inp(OPL_REG);   /* >= 80 us */
    s2 = inp(OPL_REG) & 0xE0;
    opl_escribe(0x04, 0x60);              /* y otra vez el reset     */
    opl_escribe(0x04, 0x80);
    return (s1 == 0x00) && (s2 == 0xC0);
}

/* canal 0 listo para clics: portadora a MULT 1 y modulador a MULT 15
   con nivel de salida maximo (el brillo del click son los productos
   de modulacion), envolvente de ataque inmediato (AR 15), caida corta
   (DR 9: unos 10 ms; con DR 15 el click dura decimas de ms, unas pocas
   muestras, y apenas se oye) y salida rapida (RR 15), sostenido al
   fondo (SL 15) */
static void opl_canal(void)
{
    opl_escribe(0x01, 0x20);              /* formas de onda del OPL2  */
    opl_escribe(0x20, 0x0F);              /* modulador: MULT 15       */
    opl_escribe(0x23, 0x01);              /* portadora: MULT 1        */
    opl_escribe(0x40, 0x00);              /* modulador: nivel maximo  */
    opl_escribe(0x43, 0x1D);              /* portadora: nivel justo   */
    opl_escribe(0x60, 0xF9);              /* AR 15 / DR 9 (modulador) */
    opl_escribe(0x63, 0xF9);              /* AR 15 / DR 9 (portadora) */
    opl_escribe(0x80, 0xFF);              /* SL 15 / RR 15 (modulador)*/
    opl_escribe(0x83, 0xFF);              /* SL 15 / RR 15 (portadora)*/
    opl_escribe(0xE0, 0x00);              /* seno (modulador)        */
    opl_escribe(0xE3, 0x00);              /* seno (portadora)         */
    opl_escribe(0xC0, 0x00);              /* FM, sin realimentacion   */
    opl_escribe(0xB0, 0x00);              /* canal apagado            */
}

/* el click: F-numero nuevo y key on; la caida de la envolvente (DR 9)
   lo deja en silencio en unos pocos ms, listo para el siguiente.  El
   tic (pulsos pares) es mas agudo que el tac; el bloque 4 lleva la
   portadora a ~510/315 Hz y el modulador a x15 (~7,6/4,7 kHz) */
static void tic_tac(unsigned long pulso)
{
    unsigned fnum = (pulso & 1ul) ? 0x1A0u : 0x2A0u;
    unsigned alto = (fnum >> 8) & 3u;

    opl_escribe(0xA0, (unsigned char)(fnum & 0xFF));
    opl_escribe(0xB0, (unsigned char)(alto | 0x10u));        /* key off */
    opl_escribe(0xB0, (unsigned char)(alto | 0x10u | 0x20u)); /* key on */
}

static void opl_silencio(void)
{
    opl_escribe(0xB0, 0x00);              /* key off del canal 0 */
    opl_escribe(0x04, 0x60);              /* reset de los timers */
    opl_escribe(0x04, 0x80);
}

/* ==================== tic-tac del altavoz (FALLBACK) ==================== */

/* Si no hay YM3812, el tic-tac sale por el altavoz del PC: canal 2
   del PIT (8253/8254) al puerto 42h y bits 0 (puerta) y 1 (altavoz)
   del 61h.  Sin esperas activas ni interrupciones: el pitido se
   enciende al cruzar cada cuarto de segundo (angulos() ya calcula
   t256; el contador es t256 >> 6) y se apaga EN EL TIC SIGUIENTE del
   BIOS: ~55 ms de tono, ~195 ms de silencio.  Tic y tac alternan la
   frecuencia (paridad del contador).  Los divisores del PIT son
   1193182 / frecuencia: van como CONSTANTES de 16 bits (lo primero el
   byte bajo, luego el alto, tras el comando B6h).  En un 386 la
   division seria casi gratis, pero mantener las constantes deja el
   codigo identico al del ROLEX.C de 8086 y no cambia nada. */
static void pitido(unsigned cuarto)
{
    outp(0x43, 0xB6);                       /* canal 2, modo 3, lo/hi */
    if (cuarto & 1u) {                      /* tac: 900 Hz            */
        outp(0x42, 0x2E); outp(0x42, 0x05); /* 1326 = 1193182/900    */
    } else {                                /* tic: 1000 Hz           */
        outp(0x42, 0xA9); outp(0x42, 0x04); /* 1193 = 1193182/1000   */
    }
    outp(0x61, inp(0x61) | 0x03);           /* puerta del canal 2 + altavoz */
}

/* apaga el altavoz dejando intactos los demas bits del puerto 61h */
static void callar(void)
{
    outp(0x61, inp(0x61) & ~0x03);
}

/* ==================== salida: franja en la VRAM de texto ==================== */

/* Al salir (ESC) no se llama al DOS: la salida se pinta DIRECTO en la
   VRAM de texto del modo 3 (0xB8000, mapeada 1:1 bajo DPMI), como
   franja de las 80 columnas completas de la fila 0, atributo 0x1F
   (fondo azul, texto blanco) y rellena con espacios; luego se coloca
   el cursor en la fila 1, columna 0 con la int 10h AH=02h.  El modo 3
   ya lo ha limpiado todo el BIOS.  \x82 = e con acento en
   CP437/CP850 (el fuente va en UTF-8). */
static void franja_salida(void)
{
    static const char MENSAJE[] =
        " ROLEX386 v2 (c) Copyright 2026 by Javier Guti\x82rrez Chamorro (Guti) ";
    volatile unsigned short *v = (volatile unsigned short *)0xB8000ul;
    const char *p = MENSAJE;
    int i;

    for (i = 0; i < 80; i++) {
        unsigned char c = (unsigned char)*p;
        if (c != 0) p++;
        else c = ' ';
        v[i] = (unsigned short)((0x1F << 8) | c);
    }
    {
        union REGS r;
        r.h.ah = 0x02;                    /* colocar el cursor       */
        r.h.bh = 0;                       /* pagina 0                */
        r.h.dh = 1;                       /* fila 1                  */
        r.h.dl = 0;                       /* columna 0               */
        int386(0x10, &r, &r);
    }
}

/* ==================== montaje de la imagen ==================== */

/* Descompresion RLE del fondo DIRECTO a la VRAM lineal.  Los pixeles
   son indices 0..63 (el DAC es de 6 bits: el indice es el nivel de
   gris) y cada byte lleva el valor en los bits 0..5 y (cuenta-1) en los
   bits 6..7:
       cuenta 1..3  el propio byte (bits 6..7 = 0, 1, 2)
       bits 6..7 = 3  racha de 4..255 pixeles: la cuenta va en el byte
                      siguiente
   La foto es un tren lineal de pixeles y las rachas pueden cruzar el
   borde de una fila de la franja, de ahi que el cursor se parta en los
   bordes de fila al avanzar.  (Verificado: decodifica exactamente
   STRIP_W x TIRA_H = 494 x 400 = 197.600 pixeles.) */
static void montar(void)
{
    const unsigned char *p   = FONDO_RLE;
    const unsigned char *fin = FONDO_RLE + FONDO_RLE_LEN;
    unsigned char *d    = pVideo + (size_t)TIRA_Y * VGA256_WIDTH + TIRA_X;
    unsigned char *fila = d + STRIP_W;       /* fin de la fila actual     */
    unsigned char *ult  = d + (size_t)(TIRA_H - 1) * VGA256_WIDTH + STRIP_W;
                                             /* fin de la ultima fila     */
    unsigned char *e = ESFERA;
    int f;

    while (p < fin && d < ult) {
        unsigned char v = *p++;
        unsigned n = (v >> 6) + 1u;     /* 1..3 pixeles, o 4 = cuenta aparte */
        unsigned q;

        if (n == 1u) {                  /* pixel suelto (el caso comun) */
            *d++ = v;
            if (d == fila) {
                fila += VGA256_WIDTH;
                d = fila - STRIP_W;
            }
            continue;
        }
        if (n == 4u) n = *p++;
        v &= 63;
        while (n > 0 && d < ult) {      /* la racha puede cruzar filas */
            q = (unsigned)(fila - d);
            if (q > n) q = n;
            memset(d, v, q);
            d += q;
            n -= q;
            if (d == fila) {            /* fila completa: a la siguiente */
                fila += VGA256_WIDTH;
                d = fila - STRIP_W;
            }
        }
    }

    /* esfera limpia (340x340): recorte del propio framebuffer lineal,
       como en el original */
    d = pVideo + (size_t)DIAL_YF * VGA256_WIDTH + DIAL_XF;
    for (f = 0; f < DIAL_H; f++, e += DIAL_W, d += VGA256_WIDTH)
        memcpy(e, d, DIAL_W);

    /* fondo de la ventanita tal como sale de la foto: es la fuente de la
       lente antes de ponerle la fecha (la foto no trae lupa) */
    e = ESFERA + (LUP_SY0 * DIAL_W + LUP_SX0);
    d = lup_foto;
    for (f = 0; f < LUP_SH; f++, e += DIAL_W, d += LUP_SW)
        memcpy(d, e, LUP_SW);
}

/* paleta de 256 colores al DAC (6 bits: /4 como en el original; el
   fondo es una rampa de 64 grises, indice = nivel del DAC, y los
   indices 250..255 los colores de las agujas y de la fecha) */
static void pon_paleta(void)
{
    unsigned char pal[768];
    int i;
    for (i = 0; i < 768; i++)
        pal[i] = (unsigned char)(PALETA[i] >> 2);
    VGA256SetPalette(pal);
}

/* ==================== bucle principal ==================== */

/* fotograma en el marco de RAM.  Primero devuelve a su estado de esfera
   limpia (copiando de ESFERA) las franjas que pintaron las agujas del
   fotograma anterior; despues pinta las nuevas, anotando sus franjas en
   el otro juego. */
static void compone(unsigned a_h, unsigned a_m, unsigned a_s)
{
    const int *pmin = suc_min[act], *pmax = suc_max[act];
    unsigned char *m = MARCO, *e = ESFERA;
    int y, x1;

    for (y = 0; y < DIAL_H; y++, m += DIAL_W, e += DIAL_W) {
        if (pmax[y] >= pmin[y]) {
            x1 = pmin[y] & ~3;          /* tramo alineado a dword */
            memcpy(m + x1, e + x1, (size_t)((pmax[y] | 3) - x1 + 1));
        }
    }
    act ^= 1;
    cmin = suc_min[act];
    cmax = suc_max[act];
    memset((char *)cmin, 0x7F, sizeof(suc_min[0]));
    memset((char *)cmax, 0x80, sizeof(suc_max[0]));

    baston2(a_h, R_AG_H, COL_H, HW_H);
    baston2(a_m, R_AG_M, COL_M, HW_M);
    segundero(a_s);
    centro();
    if (lupa_toca()) lupa(a_m, a_s);
}

/* vuelca a la VRAM lo que cambia: por fila, la union de las franjas del
   fotograma anterior (hay que borrarlas) y del nuevo (hay que
   pintarlas).  Se llama justo tras VGA256WaitVRetrace, asi que en la
   pantalla nunca aparece un fotograma a medias.  Los tramos van
   alineados a dword (DIAL_W, DIAL_XF y el ancho de pantalla son
   multiplos de 4, ver el #error de arriba). */
static void volcar(void)
{
    const int *pmin = suc_min[act ^ 1], *pmax = suc_max[act ^ 1];
    unsigned char *m = MARCO;
    unsigned char *v = pVideo + (size_t)DIAL_YF * VGA256_WIDTH + DIAL_XF;
    int y, x1, x2;

    for (y = 0; y < DIAL_H; y++, m += DIAL_W, v += VGA256_WIDTH) {
        x1 = cmin[y]; if (pmin[y] < x1) x1 = pmin[y];
        x2 = cmax[y]; if (pmax[y] > x2) x2 = pmax[y];
        if (x2 >= x1) {
            x1 &= ~3;
            memcpy(v + x1, m + x1, (size_t)((x2 | 3) - x1 + 1));
        }
    }
}

/* convierte tics del BIOS en angulos (1/256 de vuelta) de las agujas.
   El PIT del BIOS marca 1.193182 MHz / 65536 = 18.2065 tics/s, de
   modo que segundos = tics * 2000 / 36413 (exacto, sin deriva) y
   r = resto en unidades de 1/36413 s, que da el barrido suave.
   El tiempo en 1/256 de segundo, t256 = seg*256 + sub, da los tres
   angulos (el contador del BIOS se pone a cero a medianoche:
   seg < 86400):
       segundero  = (t256 / 60)    & 255      (60 s por vuelta)
       minutero   = (t256 / 3600)  & 255      (1 h por vuelta)
       horario    = (t256 / 43200) & 255      (12 h por vuelta)
   Solo se llama cuando cambia el tic.  Ademas de los tres angulos
   devuelve el PULSO del tic-tac: t256 >> 6, o sea 4 por segundo (el
   ritmo de un automatico a 14.400 alternancias/hora; los de 28.800
   dan 8), que suena siempre que el pulso cambia.  (Ya no se lee el
   contador del PIT para afinar dentro del tic: el BIOS lo programa en
   modo 3 y su cuenta barre 65536..0 DOS veces por tic, y como esto se
   llama justo al cambiar el tic, la fraccion salia casi siempre 0.) */
static void angulos(unsigned long tk, unsigned *a_h, unsigned *a_m,
                    unsigned *a_s, unsigned long *pulso)
{
    unsigned long t2k = tk * 2000UL;
    unsigned long s = t2k / 36413UL;
    unsigned long t256 = (s << 8) + (((t2k - s * 36413UL) << 8) / 36413UL);
    unsigned long vs = t256 / 60UL;
    unsigned long vm = vs / 60UL;

    *a_s = (unsigned)vs & 255u;
    *a_m = (unsigned)vm & 255u;
    *a_h = ((unsigned)vm / 12u) & 255u;
    *pulso = t256 >> 6;
}

int main(void)
{
    unsigned long tk, tk_ant, pulso, p_pulso = 0xFFFFFFFFul;
    unsigned a_h, a_m, a_s;
    unsigned p_h = 0xFFFFu, p_m = 0xFFFFu, p_s = 0xFFFFu;  /* ya dibujados */
    int modo, opl;
    int sonando = 0;                    /* 1 = el altavoz sigue encendido (fallback) */

    if (!VBE_Init()) {
        cputs("ROLEX386: no hay BIOS SVGA VESA (VBE).\r\n");
        return 1;
    }
    modo = VBE_FindMode(VGA256_WIDTH, VGA256_HEIGHT, 8);
    if (modo < 0 || !VBE_IsModeLinear(modo)) {
        VBE_Done();
        cputs("ROLEX386: se necesita un modo lineal VGA256 (640x400x256).\r\n");
        return 1;
    }
    VBE_SetMode(modo, 1, 1);              /* lineal y limpiar pantalla */
    pVideo = (unsigned char *)VBE_GetVideoPtr(modo);
    if (pVideo == NULL) {
        VBE_SetMode(3, 0, 0);
        VBE_Done();
        cputs("ROLEX386: el DPMI no puede mapear la memoria de video.\r\n");
        return 1;
    }

    /* paleta ANTES de blitear la foto, para que no asome ni un frame
       con la paleta por defecto del VBE */
    init_sincos();
    init_lupa();
    pon_paleta();

    montar();

    pinta_fecha(lee_fecha());

    /* tic-tac: PRIORIDAD FM.  Si hay OPL2 sale por FM; si no,
       FALLBACK al altavoz del PC (pitido/callar).  opl decide una
       sola vez y queda fijo para todo el bucle. */
    opl = opl_presente();
    if (opl) opl_canal();

    tk_ant = lee_ticks() - 1;            /* != tk: el primer fotograma sale ya */

    for (;;) {
        tk = lee_ticks();
        if (tk == tk_ant) continue;            /* mismo tic: nada que hacer */
        while (hay_tecla()) {                  /* el teclado, una vez por tic */
            if (lee_tecla() == 27) {     /* ESC -> salir */
                if (opl) opl_silencio(); else callar();   /* que no quede sonido */
                VBE_SetMode(3, 0, 0);
                VBE_Done();
                franja_salida();         /* franja azul/blanco en la fila 0 */
                return 0;
            }
            /* cualquier otra tecla se descarta del buffer */
        }
        if (tk < tk_ant) {                     /* cruzo la medianoche */
            pinta_fecha(lee_fecha());
        }
        tk_ant = tk;
        angulos(tk, &a_h, &a_m, &a_s, &pulso);
        if (sonando) {                         /* un tic despues: a callar */
            callar();
            sonando = 0;
        }
        if (pulso != p_pulso) {                /* pulso nuevo: tic-tac */
            p_pulso = pulso;
            if (opl) tic_tac(pulso);
            else {
                pitido((unsigned)pulso);
                sonando = 1;
            }
        }
        if (a_h == p_h && a_m == p_m && a_s == p_s) continue;  /* igual */
        p_h = a_h; p_m = a_m; p_s = a_s;
        compone(a_h, a_m, a_s);
        VGA256WaitVRetrace();                  /* y SOLO ahora, a la VRAM */
        volcar();
    }
}