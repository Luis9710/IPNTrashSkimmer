using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Threading;
using System.IO;
using AForge.Video.DirectShow;
using AForge.Video;
using System.Reflection;

namespace PruebaCSharp
{
    public partial class Form1 : Form
    {
        //Variables para transmisión de video
        private bool EncendidoVideo;
        private FilterInfoCollection MiDispositivos;
        private VideoCaptureDevice MiWebCam;
        //Variables para transmisión de datos
        bool process = false;
        bool statusPort = false;
        bool EncendidoDatos;
        bool Botones;
        bool Banda = false;
        bool bandera_write = false;
        SerialPort serialPort;
        int baudRate = 19200;
        int dt = 100;

        StreamWriter fichero; //Clase que representa un fichero

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            CargaDispositivos();
            string[] ports = SerialPort.GetPortNames();
            foreach (string port in ports)
            {
                CB_Puertos.Text = port;
            }
            EncendidoVideo = false;
            EncendidoDatos = false;
            GB_Inclinacion.Visible = false;
            PB_Bateria.Visible = false;
            LB_Bateria.Visible = false;
            LB_TempExt.Visible = false;
            LB_TempInt.Visible = false;
            LB_Flota.Visible = false;
            IniciarComunicacion();
            GenerarTxt();
            timer1.Start();
        }

        //-------------Transmisión de video---------------------
        public void CargaDispositivos()
        {
            MiDispositivos = new FilterInfoCollection(FilterCategory.VideoInputDevice);
            if (MiDispositivos.Count > 0)
            {
                for (int i = 0; i < MiDispositivos.Count; i++)
                    CB_DispositivoVideo.Items.Add(MiDispositivos[i].Name.ToString());
                CB_DispositivoVideo.Text = MiDispositivos[0].ToString();
            }
        }

        public void CerrarWebCam()
        {
            if (MiWebCam != null && MiWebCam.IsRunning)
            {
                MiWebCam.SignalToStop();
                MiWebCam = null;
            }
        }
        private void Capturando(object sender, NewFrameEventArgs eventArgs)
        {
            Bitmap Imagen = (Bitmap)eventArgs.Frame.Clone();
            PB_Video.Image = Imagen;
        }
        private void Btn_EAVideo_Click(object sender, EventArgs e)
        {
            CerrarWebCam();
            int i = CB_DispositivoVideo.SelectedIndex;
            try
            {
                string NombreVideo = MiDispositivos[i].MonikerString;
                MiWebCam = new VideoCaptureDevice(NombreVideo);
                MiWebCam.NewFrame += new NewFrameEventHandler(Capturando);
                if (EncendidoVideo == false)
                {
                    MiWebCam.Start();
                    EncendidoVideo = true;
                    Btn_EAVideo.Text = "Apagar video";
                    MandarComando("C1");
                }
                else
                {
                    MiWebCam.Stop();
                    EncendidoVideo = false;
                    PB_Video.Image = null;
                    Btn_EAVideo.Text = "Encender video";
                    MandarComando("C0");
                }
            }
            catch
            {
                MessageBox.Show("Dispositivo de video no válido");
            }


        }

        //------------Transmisión de datos----------------------

        private bool InitSer()
        {
            bool val;
            try
            {
                serialPort = new SerialPort(CB_Puertos.Text);
                serialPort.BaudRate = baudRate;
                serialPort.WriteTimeout = 50;
                serialPort.Open();
                val = true;
            }
            catch
            {
                val = false;
            }
            return val;
        }

        private void IniciarComunicacion()
        {
            statusPort = InitSer();
            Thread.Sleep(dt);
            try
            {
                serialPort.DiscardOutBuffer();
                //serialPort.WriteLine("start");
                process = true;
            }
            catch
            {
                MessageBox.Show("Error al mandar datos por el puerto serial");
                process = false;
                LB_Estado.Text = "No hay comunicación con USV";
            } 
        }
        
        delegate void CambiarDatosDelegado(string V1, string V2, string V3, string V4, string V5);

        private void CambiarDatos(string V1, string V2, string V3, string V4, string V5)
        {
            if (this.InvokeRequired) 
            { 
                CambiarDatosDelegado delegado = new CambiarDatosDelegado(CambiarDatos);
                object[] parametros = new object[] {V1, V2, V3, V4, V5};
                this.Invoke(delegado, parametros);
            }
            else
            {
                LB_Bateria.Text = "Batería: " + V1 + " V";
                LB_TempExt.Text = "Temperatura interna: " + V2 + " °C";
                LB_TempInt.Text = "Temperatura interna: " + V3 + " °C";
                //LB_Flota.Text = "Nivel de flotabilidad: " + V4 + " %";
                LB_X.Text = "X: " + V4 + " °";
                LB_Y.Text = "Y: " + V5 + " °";

                if (bandera_write == true)
                {
                    fichero.WriteLine(V1 + "," + V2 + "," + V3 + "," + V4 + "," + V5);
                    bandera_write = false;
                }
            }
        }
        
        private void RecepcionDatos()
        {
            while(EncendidoDatos)
            {
                try
                { 
                    string reading = serialPort.ReadLine();
                    string[] splitData = reading.Split(new char[] { ',' });
                    string NBateria, Tmext, Tmint, NFlota, X, Y;
                    if (splitData.Length == 5)
                    {
                        try
                        {
                            NBateria = splitData[0];
                            Tmext = splitData[1];
                            Tmint = splitData[2];
                            //NFlota = splitData[3];
                            X = splitData[3];
                            Y = splitData[4];
                            CambiarDatos(NBateria, Tmext, Tmint, X, Y);
                            //Thread.Sleep(dt);
                        }
                        catch
                        {

                        }
                    }
                }
                catch 
                { 
                
                }
                try { serialPort.DiscardInBuffer(); }
                catch { }
            }
        }

        private void Btn_EADatos_Click(object sender, EventArgs e)
        {
            ThreadStart delegado = new ThreadStart(RecepcionDatos);
            Thread hilo_recepcion = new Thread(delegado);

            if (EncendidoDatos==false && process)
            {
                GB_Inclinacion.Visible = true;
                PB_Bateria.Visible = true;
                LB_Bateria.Visible = true;
                LB_TempExt.Visible = true;
                LB_TempInt.Visible = true;
                LB_Flota.Visible = true;
                EncendidoDatos = true;
                LB_Estado.Text = "Recibiendo datos";
                Btn_EADatos.Text = "Apagar recepción de datos";
                hilo_recepcion.Start();
            }
            else
            {
                GB_Inclinacion.Visible = false;
                PB_Bateria.Visible = false;
                LB_Bateria.Visible = false;
                LB_TempExt.Visible = false;
                LB_TempInt.Visible = false;
                LB_Flota.Visible = false;
                EncendidoDatos = false;
                Btn_EADatos.Text = "Encender recepción de datos";
                LB_Estado.Text = "Esperando comandos";
                hilo_recepcion.Abort();
            }
        }

        private void Btn_Avanzar_Click(object sender, EventArgs e)
        {
            EstadoAvanzar();
        }

        private void Btn_Derecha_Click(object sender, EventArgs e)
        {
            EstadoDerecha();
        }

        private void Btn_Retroceso_Click(object sender, EventArgs e)
        {
            EstadoRetroceso();
        }

        private void Btn_Izquierda_Click(object sender, EventArgs e)
        {
            EstadoIzquierda();
        }

        private void Btn_Banda_Click(object sender, EventArgs e)
        {
            if (Banda == false)
            {
                Banda = true;
                EstadoBanda();
                LB_Estado.Text = "Recolectando basura";
                LB_MB.BackColor = Color.Lime;
            }
            else
            {
                Banda = false;
                EstadoBanda();
                LB_Estado.Text = "Basura ya recolectada";
                LB_MB.BackColor = Color.Silver;
            }

        }

        private void Btn_Paro_Click(object sender, EventArgs e)
        {
            EstadoParo();
        }

        private void Form1_Closing(object sender, FormClosingEventArgs e)
        {
            CerrarWebCam();
            if (process)
            {
                serialPort.DiscardInBuffer();
                serialPort.Close();
            }
            fichero.Close();
        }

        private void Btn_Aceptar_Click(object sender, EventArgs e)
        {
            int v;
            string velocidad;
            if (CB_Control.Text == "Botones gráficos")
            {
                Btn_Avanzar.Enabled = true;
                Btn_Banda.Enabled = true;
                Btn_Derecha.Enabled = true;
                Btn_Izquierda.Enabled = true;
                Btn_Retroceso.Enabled = true;
                Btn_Paro.Enabled = true;
                Botones = true;
            }
            else if (CB_Control.Text == "Teclado PC")
            {
                Btn_Avanzar.Enabled = false;
                Btn_Banda.Enabled = false;
                Btn_Derecha.Enabled = false;
                Btn_Izquierda.Enabled = false;
                Btn_Retroceso.Enabled = false;
                Btn_Paro.Enabled = false;
                Botones = false;
            }
            v = (int)NUP_Velocidad.Value * 250/100;
            velocidad = v.ToString();
            MandarComando("V"+velocidad);
            //Thread.Sleep(100);
            //MandarComando(velocidad);
        }

        private void Btn_Control_KeyDown(object sender, KeyEventArgs e)
        {
            if (Botones == false)
            {
                switch (e.KeyCode)
                {
                    case Keys.Up:
                        EstadoAvanzar();
                        break;
                    case Keys.Down:
                        EstadoRetroceso();
                        break;
                    case Keys.Right:
                        EstadoDerecha();
                        break;
                    case Keys.Left:
                        EstadoIzquierda();
                        break;
                    case Keys.Space:
                        EstadoParo();
                        break;
                    case Keys.Enter:
                        EstadoBanda();
                        break;
                }
            }
        }

        private void MandarComando(string Comando)
        {
            try
            {
                serialPort.DiscardInBuffer();
                serialPort.DiscardOutBuffer();
                Thread.Sleep(10);
                serialPort.Write(Comando);
                Thread.Sleep(10);
            }
            catch (Exception e)
            {

                // MessageBox.Show("Error al mandar datos por el puerto serial");
                MessageBox.Show(e.Message);

            }
        }

        private void EstadoAvanzar()
        {
            MandarComando("A");
            LB_Estado.Text = "Avanzando";
            LB_MI.BackColor = Color.Lime;
            LB_MD.BackColor = Color.Lime;
            LB_MB.BackColor = Color.Silver;
        }

        private void EstadoDerecha()
        {
            MandarComando("D");
            LB_Estado.Text = "Girando a la derecha";
            LB_MI.BackColor = Color.Silver;
            LB_MD.BackColor = Color.Lime;
            LB_MB.BackColor = Color.Silver;
        }

        private void EstadoRetroceso()
        {
            MandarComando("R");
            LB_Estado.Text = "Retrocediendo";
            LB_MI.BackColor = Color.DarkOliveGreen;
            LB_MD.BackColor = Color.DarkOliveGreen;
            LB_MB.BackColor = Color.Silver;
        }

        private void EstadoIzquierda()
        {
            MandarComando("I");
            LB_Estado.Text = "Girando a la izquierda";
            LB_MI.BackColor = Color.Lime;
            LB_MD.BackColor = Color.Silver;
            LB_MB.BackColor = Color.Silver;
        }

        private void EstadoBanda()
        {
            MandarComando("B");
        }

        private void EstadoParo()
        {
            MandarComando("P");
            LB_Estado.Text = "Detenido";
            LB_MI.BackColor = Color.Silver;
            LB_MD.BackColor = Color.Silver;
            LB_MB.BackColor = Color.Silver;
        }

        private void GenerarTxt()
        {
            fichero = File.CreateText("prueba.dat"); //Creamos un fichero
        }

        private void Timer_Tick(object sender, EventArgs e)
        {
            bandera_write = true;
        }
    }
}
