/***************************************************************

  FlexNet / BayCom        Layer 1 Service Interface

  (C) 7/1994 DK7WJ, DL8MBT
  Translation by HB9JNX

  Definitionen und Prototypes fÅr Aufruf von Layer1-Funktionen
  Definitions and prototypes for calling Layer1 functions


  28.08.94   DK7WJ          Erste verteilte Version
                            First distributed version
  10.09.94   DK7WJ          Modebefehl geaendert
                            Changed the mode command
                            Prototypen neu f. Speichermodell MEDIUM
                            New prototypes for the MEDIUM memory model
  25.09.94   DK7WJ          FDX an Treiber uebergeben
                            FDX now forwarded to the driver
  20.02.95   DK7WJ          CH_TBY neu
                            CH_TBY added
  31.03.95   DK7WJ          Device-Versionkennungen
                            Device version identifier
  01.03.96   DK7WJ          Windows extensions, comments pending :-)
  22.04.96   DK7WJ          .
  25.03.97   HB9JNX         IO trapping changes

  10.01.00   DK7WJ          WIN32 definitions

  19.06.00   DK7WJ          CH_DEAD included
 ***************************************************************/

//#ifndef _FLEXTYPES_DEFINED
//#define _FLEXTYPES_DEFINED
typedef unsigned char   byte;
typedef signed char     i8;
typedef signed short      i16;
typedef unsigned short    u16;
typedef unsigned long   u32;
#ifdef _WIN32
#define far
#define near
#endif
//#endif

#ifdef _WIN32
#pragma pack(push, flexdrv)
#pragma pack(1)
#endif


#define MAXFLEN 400     /* Maximale Laenge eines Frames */
                        /* maximum length of a frame */

/* Struct f. Treiberkommunikation bei TX und RX */
/* struct for communicating RX and TX packets to the driver */
typedef struct
    {
    i16  len;               /* Laenge des Frames - length of the frame */
    byte kanal;             /* Kanalnummer - channel number */
    byte txdelay;           /* RX: Gemessenes TxDelay [*10ms],
                                   0 wenn nicht unterstuetzt
                               TX: Zu sendendes TxDelay */
                            /* RX: measured transmitter keyup delay (TxDelay) in 10ms units,
                                   0 if not supported
                               TX: transmitter keyup delay (TxDelay) that should be sent */
    byte frame[MAXFLEN];    /* L1-Frame (ohne CRC) - L1 frame without CRC */
    } L1FRAME;

/* Struct f. Kanalstatistik (wird noch erweitert) */
/* struct for channel statistics (still being extended) */
#ifndef _FLEXAPPL
typedef struct
    {
    u32 tx_error;           /* Underrun oder anderes Problem - underrun or some other problem */
    u32 rx_overrun;         /* Wenn Hardware das unterstuetzt - if supported by the hardware */
    u32 rx_bufferoverflow;
    u32 tx_frames;          /* Gesamt gesendete Frames - total number of sent frames */
    u32 rx_frames;          /* Gesamt empfangene Frames - total number of received frames */
    u32 io_error;           /* Reset von IO-Device - number of resets of the IO device */
    u32 reserve[4];         /* f. Erweiterungen, erstmal 0 lassen! - reserved for extensions, leave 0! */
    } L1_STATISTICS;
#endif

#ifdef _WIN32
extern HANDLE hInst;
#pragma pack(pop, flexdrv)
#endif

/* Masken fÅr den Mode-Parameter - bitmasks for the mode parameter */
#define MODE_d   0x0080
#define MODE_r   0x0040
#define MODE_t   0x0020
#define MODE_z   0x0010
#define MODE_p	 0x0004	  /* needs also MODE_d */
#define MODE_c   0x0002
#define MODE_off 0x0001   /* Special: Wenn 1, Kanal abgeschaltet */
                          /* special: if one, the channel is switched off */

/* Masken fÅr den L1-Kanalstatus */
/* masks for the L1 channel status */
#define CH_DEAD 0x01
#define CH_RXB  0x40
#define CH_PTT  0x20
#define CH_DCD  0x10
#define CH_FDX  0x08
#define CH_TBY  0x04

#ifdef _WIN32
// Konfiguration. Wenn geaendert, TRUE returnen, dann erfolgt exit() und Re-Init
// max_channels: Maximal moegliche Kanalanzahl f. diesen Treiber
int config_device(byte max_channels, HWND hParentWindow, byte channel);
// Return: Anzahl belegte Kanaele!

byte *config_info(byte channel);
/* Liefert String der die treiberspezifische Konfiguration beschreibt, z.B. Resourcen-Name
   Wenn keine treiberspezifische Konfiguration vorhanden (config_device() ist Dummy),
	Nullpointer liefern */
/* Returns a string describing the channel configuration, i.e. the name of hardware resource
   If config_device() is a dummy, this call must return a null pointer
*/
int init_device(HKEY kHey);

void far l1_exit(HKEY hKey);

void set_txdelay(byte channel, byte txdel);
byte get_txdelay(byte channel);
u16 get_baud(byte channel);
u16 get_mode(byte channel);
#else
u16 far init_device(int argc, char near *argv[]);
/* Treiberinterner Aufruf aus Treiberkopf bei Installation: kann benutzt
   werden um Command Line Parameter zu parsen
   Return: 0=OK; sonst Exit mit Returnwert als Exit-Argument
    ACHTUNG: Wenn Returnwert >0, erfolgt Abbruch, es duerfen dann also keine
             aktiven oder umgelegten Interrupts verbleiben!
*/
/* This procedure is called from the driver stub at installation time. It
   may be used to parse the command line
   Return value: 0=OK; otherwise installation is cancelled and the value
                 returned as exit argument
    WARNING: If return value >0 the installation is cancelled, active or
             patched interrupt vectors must not be left!
*/
void far l1_exit(void);
/*  Wird bei Verlassen des Programms aufgerufen und kann zum AufrÑumen
    verwendet werden (Interrupts und Schnittstellen abschalten)
*/
/*  Is called when the driver terminates and may be used to clean up everything
    (switch off interrupts and interfaces)
*/

#endif

u16 far l1_get_ch_cnt(void);
/* Aufruf aus Treiberkopf bei Programmstart; muss Anzahl der im Treiber
   definierten Kanaele liefern
*/
/* called from the driver stub at installation time; should return the number
   of channels defined by the driver
*/


byte far l1_ch_active(byte kanal);
/*  Zurueckliefern, ob Kanal im Prinzip funktionstuechtig ist.
    Also immer, ausser wenn Dummykanal, >0 liefern!
*/
/* report if the channel could work in principle. Thus always return >0 if
   not a dummy channel.
*/

byte far l1_init_kanal(byte kanal, u16 baud, u16 mode);
/*
    Kanalnummer: 0-15
    Baud: Baudrate / 100, z.B. 115200 Baud wird als 1152 uebergeben
    Mode: Kanalmodus, Bitflaggen, derzeit definiert:
         0x0080   d   Vollduplex
         0x0040   r   Externer RX-Takt
         0x0020   t   Externer TX-Takt
         0x0010   z   NRZ-Kodierung (Anstatt NRZI), nur f. HDLC
         0x0002   c   Kanal macht immer CRC, nur f. KISS
                         Baycom: Soft-DCD aktivieren
         0x0001   -   Kanal per Modebefehl deaktiviert
    Es sind noch weitere Bits definiert, die sind aber im L1-Treiber nicht
    relevant!
    Um eventuelle Verklemmungen zu beseitigen, sollte in diesem Modul der
    Treiber soweit als moeglich (re)initialisiert werden!

    Return: 1 wenn alles ok;
            0 wenn Parameterfehler, dann Anzeige von '---' in der Modeliste

    Aufruf erfolgt beim Programmstart sowie bei jeder Baud- oder ModeÑnderung,
    ausserdem wenn der Kanal seit 3 Minuten nichts mehr empfangen hat.
*/
/*
    kanal: channel number (0-15)
    baud:  the baud rate / 100, for example 115200 baud is specified as
           baud = 1152
    mode:  the channel mode, a bit field, currently the following bits are
           defined:
           0x0080   d   Full duplex
           0x0040   r   External RX clock
           0x0020   t   External TX clock
           0x0010   z   NRZ coding (instead of NRZI) (only for HDLC)
           0x0002   c   KISS: driver forces checksum
                        Baycom: activate software DCD
           0x0001   -   channel inactivated
           There are some additional bits defined which are not relevant to the
           L1 driver.

    To recover from lockups, this function should (re)initialize as much of the
    driver and the hardware as possible.

    Return value: 1 if everything is ok;
                  0 if a parameter error; '---' is then displayed in the ports
                  list

    This procedure is called at program start, at every baud rate or mode
    change, or if the driver has not received anything for 3 minutes.
*/

L1FRAME far * far l1_rx_frame(void);
/*
    Wird zyklisch aufgerufen

    Return: *Frame oder 0 wenn nichts empfangen
            Der Frame mu· jeweils bis zum nÑchsten Aufruf verfÅgbar bleiben,
            d.h. jeder Aufruf lîscht den zuletzt gemeldeten Frame
*/
/*
    Is called periodically

    Return value: *Frame or null pointer if nothing was received
                  The frame has to be readable until the next call to this
                  function, i.e. every call removes the frame returned by the
                  last call.
*/


L1FRAME far * far l1_get_framebuf(byte kanal);
/*  L2 fordert hiermit einen Sendepuffer an. Er mu· die MaximallÑnge aufnehmen
    kînnen. Wenn erfolgreich, folgt vielleicht l1_tx_frame(), es kann aber
    auch vorkommen dass ein weiterer Aufruf erfolgt! Also nicht dynamisch
    allokieren, sondern nur einen Pointer liefern!
    Return: *Frame, der Frame wird hier hineingebaut
            0 wenn kein Platz verfuegbar
*/
/* L2 requests a transmitter buffer. The buffer must be able to store the
   maximum length. If successful, a l1_tx_frame() probably follows, but
   it may also happen that another call to l1_get_framebuf() follows!
   Therefore, do not allocate dynamically, just return a pointer!
   Return value: *Frame : the frame is stored here
                 0      : no storage available
*/


byte far l1_tx_frame(void);
/*  Aussenden des Frames in vorher mit l1_get_framebuf angefordeten Buffers;
    Return: 1 wenn ok, sonst 0
    Wenn der Sender aus ist, muss jetzt die Entscheidung fallen ob gesendet
    werden kann!!!
    ACHTUNG: Wenn die Sendung nicht moeglich ist, darf der Frame auf keinen
    Fall gespeichert werden! Stattdessen den Frame verwerfen und 0 returnen.
    DCD darf den Sender auch bei Halbduplex NICHT sperren! Dies ist Bedingung
    fuer das Funktionieren des FlexNet-OPTIMA-Zugriffsverfahrens. Die DCD-
    Verriegelung ist bereits in FlexNet enthalten!
    Wenn der TX bereits an ist (PTT an) und es werden bereits Frames gesendet,
    sollte der Frame zur Sendung zwischengespeichert werden, sofern der Puffer-
    platz dafuer ausreicht. Wenn nicht, 0 returnen, dann erfolgt spaeter ein
    neuer Versuch. In diesem Fall muss jedoch sichergestellt sein, dass der
    Frame auf jeden Fall noch in DIESEM Durchgang gesendet wird!
    Das Kanalzugriffstiming findet bereits im FlexNet-Kern statt!
*/
/*  Transmit the frame in the buffer that was returned by the last
    l1_get_framebuf()
    Return value: 1 if ok, 0 if not.
    If the transmitter is off, then the decision must now be made if
    transmission is possible!!!
    WARNING: If transmission is not possible now, the frame MUST NOT be stored!
    The frame should be thrown away and zero should be returned. DCD must not
    inhibit the transmitter, not even in half duplex mode! This is a necessity
    for the FlexNet-OPTIMA-Channel access algorithm. The Kernel already
    contains the DCD transmitter inhibit.
    If the transmitter is already on (PTT keyed) and frames are being sent,
    then the frame should be stored if the buffer memory suffices. If not,
    zero should be returned, the kernel will try again later. If the frame
    is stored, it is important that it gets transmitted during the same
    transmission!
    The channel access timing is already handled by the FlexNet kernel!
*/

void far l1_tx_calib(byte kanal, byte minutes);
/*  Sofern die Hardware es erlaubt, Sender in Calibrate-Modus schalten.
    Minutes=0 stoppt den Calibrate-Mode sofort.
*/
/* If the hardware allows it, switch the transmitter into the calibrate mode
   (i.e. start transmitting a calibration pattern)
   minutes=0 should immediately stop the calibration mode.
*/

byte far l1_ch_state(byte kanal);
/* Gibt verschiedene KanalzustÑnde zurÅck, Bits sind 1 wenn wahr:
         0x40   RxB RxBuffer ist nicht leer
         0x20   PTT Sender ist an
         0x10   DCD EmpfÑnger ist aktiv
         0x08   FDX Kanal ist Vollduplex, kann also immer empfangen
         0x04   TBY Sender ist nicht bereit, z.B. wegen Calibrate

Anmerkung: Die korrekte Bedienung dieser Flags ist essentiell fuer das
Funktionieren des FlexNet-Kanalzugriffstimings incl. DAMA-Master und -Slave.

Erlaeuterungen zu den Flags:

RxB ist 1 wenn gueltige RX-Frames im Puffer liegen. Verriegelt u.a. den Sender
    bei Halbduplex, da immer erst alle Frames verarbeitet werden sollen.
    Waehrend des Empfangs muss dieses Flag mit DCD ueberlappend bedient werden,
    d.h. sobald ein RX-Frame als gueltig erkannt wird, muss RxB spaetestens
    1 werden, wenn DCD 0 wird!
PTT 1 wenn der TX bereits an ist. Dann setzt FlexNet voraus dass uebergebene
    Frames in diesem Durchgang noch gesendet werden koennen. Braucht nicht
    bedient zu werden bei reinen Vollduplextreibern, also wenn FDX 1 ist.
DCD 1 wenn der Kanal belegt ist. Darf im Treiber den Sender nicht blockieren,
    die Entscheidung ob gesendet werden kann faellt stets im FlexNet-Kern!
FDX Ist eine typabhaengige Konstante. Stets 0 bei Treibern die einen Halb-
    duplexkanal bedienen koennen, also die PTT- und DCD-Flags korrekt
    ansteuern. Treiber die kein Kanalzugriffstiming benoetigen (z.B. Ethernet,
    SLIP, KISS f. reine Rechnerkopplung usw.), liefern hier 1.
TBY Dient nur zur Beschleunigung des Kernels. Wenn 1, wird nichts gesendet.
    Kann also z.B. waehrend Calibrate oder bei abgeschaltetem Kanal gesetzt
    werden
*/
/* return different channel states. The following bits are one if true:
         0x40   RxB RxBuffer is not empty
         0x20   PTT Transmitter is keyed on
         0x10   DCD Receiver is active
         0x08   FDX Channel is full duplex, receiver can always receive
         0x04   TBY Transmitter not ready, for example because of ongoing
                    calibration

Note: It is essential that these flags are services correctly. Otherwise,
the channel access timing including DAMA master and slave will not work
correctly.

Explanation of the flags:

RxB is 1 if valid RX frames are available in the buffer. Inhibits for example
    the transmitter in half duplex mode, because all frames should be processed
    before transmitting. During receipt this flag should overlap the DCD flag,
    i.e. RxB should go to one before DCD transitions to zero!
PTT 1 if the transmitter is already keyed up. In this case, FlexNet assumes
    that frames passed are sent during the same transmission. Needs not be
    serviced by full duplex drivers (i.e. if FDX is one)
DCD 1 if the channel is busy. Must not inhibit the transmitter in the driver,
    since the decision if one should transmit is made by the FlexNet kernel!
FDX is a driver dependent constant. Always 0 for drivers that may service a
    half duplex channel, i.e. that serve the PTT and DCD flags correctly.
    Drivers that do not need a channel access timing (for example Ethernet,
    SLIP, KISS for connecting PCs and so on), should return 1.
TBY Speeds up the kernel. If 1, it will not try to send. May be set to one
    for example during calibration or if the channel is switched off.
*/

u16 far l1_scale(byte kanal);
/*  BaudratenabhÑngigen Faktor liefern, wird fÅr diverse adaptive Parms
    gebraucht, u.a. T2-Steuerung.

    Return: 300 Baud:   2048
            9600 Baud:    64

        oder anders ausgedrÅckt: Return = 614400/[Baudrate]

    Sollte die real moegliche Geschwindigkeit des Kanals liefern. Bei Treibern,
    die die in init_kanal() uebergebene Baudrate verwenden, berechnen. Treiber,
    die diese nicht verwenden, sollten einen Schaetzwert liefern. Bei sehr
    schnellen Kanaelen (z.B. Ethernet) immer 0 liefern!
*/
/* Return a baud rate dependent factor. Is needed to calculate several adaptive
   parameters, for example T2 control

   Return value: 300 baud:    2048
                 9600 baud:     64

       or as a formula: Return value = 614400/[baud rate]

  Should return the real speed of the channel. Drivers that use the baud rate
  specified in init_kanal() should calculate the return value. Drivers that do
  not use this parameter should estimate the speed of the channel. Drivers for
  very fast channels (for example Ethernet) should always return 0!
*/

char far * far l1_ident(byte kanal);
/*  Kennung fÅr Kanalhardware bzw. Treibertyp zurÅckgeben.
    Geliefert wird ein far-pointer auf einen null-terminierten String.
    Der String fÅr die Kanalhardware ist maximal 8 Zeichen lang,
    z.B. "SCC0" .. "SCC3", "RS232", "PAR96", "KISS", "IPX", usw.
*/
/*  Return an identifier for the channel hardware or the driver type.
    The return value must be a far pointer to a zero terminated string.
    The string should be at most 8 characters long.
    Examples are "SCC0" .. "SCC3", "RS232", "PAR96", "KISS", "IPX", etc.
*/

char far * far l1_version(byte kanal);
/* Versionsnummernstring liefern. Maximal 5 Zeichen
*/
/* Return a version number string. At most 5 characters.
*/

L1_STATISTICS far * far l1_stat(byte kanal, byte delete);
/*  Statistikwerte liefern
    Delete = 1: Werte auf 0 zuruecksetzen!
    Wenn nicht implementiert, Nullpointer liefern!
*/
/* Return the statistics.
   Delete = 1: clear the statistics values!
   If not implemented, return a null pointer!
*/

void set_led(byte kanal, byte ledcode);
/* Falls der Kanal Status-LEDs hat (z.B. TNC), kommen hier die Zustaende
   bei jeder Aenderung an; kann sicherlich meistens Dummy sein:
    Bitmasken dazu: */
/* If the channel hardware has status LEDs (for example a TNC), this procedure
   may be used to drive them; in most cases, this procedure may be a dummy.
    Bitmasks used: */
#define LED_CON     2       /* Mindestens 1 Connect - at least one connect */
#define LED_STA     4       /* Sendeseitig ausstehende Frames - frames in the transmitter buffer */

/*---------------------------------------------------------------------------*/
#ifndef _WIN32
/* Pointer auf unsigned-Timertic, f. schnellen Zugriff wenn Zeitintervalle
   geprueft werden muessen. Inkrementiert alle 100ms mit Phasenjitter */
/* Pointer to an unsigned timer_tic, for fast access if timing intervals
   need to be measured. Increments every 100ms with phase jitter */
extern const volatile far u16 *timer_tic;

/* Variable fuer externe Kommunikation zwischen Modulen
   Koennen bei Bedarf besetzt werden */
/* Variables for intermodule communications
   May be used if needed */
extern u16 device_type;     /* Bitte vor Vergabe DK7WJ konsultieren! */
                            /* Please consult DK7WJ prior to usage! */
extern u16 device_version;  /* Versionskennung - version identifier */
extern void far *device_vector; /* Beliebiger Vektor, wird zurueckgeliefert */
                                /* arbitrary vector returned by device_vec() */
extern void far (*drv_cb)(void);
typedef struct
    {
    u16 ds;
    u16 es;
    u16 cs;
    u16 dsize;
    u16 esize;
    u16 csize;
    u16 offset;
    u16 irq;
    u16 revision;
    u16 ioport1;
    u16 ioextent1;
    u16 ioport2;
    u16 ioextent2;
    } defint;
extern defint intsegs;

#define DEFINT_REVISION 1

/*---------------------------------------------------------------------------*/
/*     Die folgenden Aufrufe werden durch den Treiberkern bereitgestellt     */
/*        The following procedures are supplied by the driver library        */
/*---------------------------------------------------------------------------*/
/* Einige einfache Ausgaberoutinen, die die Verwendung von printf() ersparen */
/* Nur waehrend der Initialisierung in init_device() verwenden!!             */
/* Some simple console output routines, that make printf() unnecessary       */
/* May only be used during initialisation, inside init_device()!!            */
void far pch(byte character);
/*  Gibt ein Zeichen auf die Konsole aus
    Print a character onto the console
*/
void far pstr(byte near *string);
/*  Gibt einen C-String auf die Konsole aus
    Print a C string onto the console
*/
void far pnum(u16 num);
/*  Gibt einen numerischen Wert dezimal aus
    Print a decimal numerical value onto the console
*/
void far phex(u16 hex);
/*  Gibt einen numerischen Wert hexadezimal aus
    Print a hexadecimal numerical value onto the console
*/
#endif // _WIN32
