/****************************************************************
* FILENAME:     PER_int.c
* DESCRIPTION:  periodic interrupt code
* AUTHOR:       Mitja Nemec
*
****************************************************************/
#include    "PER_int.h"
#include    "TIC_toc.h"

enum    OUT_STATE out_control = PI_REP;
bool    amp_control = FALSE;

// za oceno obremenjenosti CPU-ja
float   cpu_load  = 0.0;
long    interrupt_cycles = 0;

// temperatura procesorja
float	cpu_temp = 0.0;
float	napetost = 0.0;

// tokovi
float   i_s = 0.0;
float   i_f = 0.0;
float	i_out = 0.0;

float   i_s_offset = 2048;
float   i_f_offset = 2048;
float	i_out_offset = 2048;

float   i_s_gain = (5.0 / 0.625 ) * (7.5 / 6.2) * (3.3 / 4096);
float   i_f_gain = (25.0 / 0.625 ) * (7.5 / 6.2) * (3.3 / 4096);
float	i_out_gain = (25.0 / 0.625 ) * (7.5 / 6.2) * (3.3 / 4096);

long    current_offset_counter = 0;

float	i_f_zeljen = 0.0;

// napetosti
float   u_ac = 0.0;
float   u_dc = 0.0;
float   u_f = 0.0;
float   u_out = 0.0;

// zacetni offseti meritev
float   u_ac_offset = 2048.0;
float   u_out_offset = 2048.0;
float   u_dc_offset = 2048.0;
float   u_f_offset = 2048.0;

float   u_ac_gain = -((1000 + 0.47) / (5 * 0.47)) * (3.3 / 4096);
float   u_dc_gain = ((330 + 1.8) / (5 * 1.8)) * DEL_UDC_CORR_F * (3.3 / 4096);
float   u_f_gain = -((330 + 1.8) / (5 * 1.8)) * (3.3 / 4096);
float   u_out_gain = ((1000 + 0.47) / (5 * 0.47)) * (3.3 / 4096);

// filtrirana napetost DC linka
DC_float    u_dc_f = DC_FLOAT_DEFAULTS;
float   u_dc_filtered = 0.0;

// prvi harmonik in RMS vhodne omrežne napetosti (u_ac)
DFT_float   u_ac_dft = DFT_FLOAT_DEFAULTS;
float   u_ac_rms = 0.0;
float   u_ac_form = 0.0;
float	u_ac_zeljena = 0.0;
float	u_ac_kot = 0.0;

// prvi harmonik in RMS vhodne omrežne napetosti (u_ac)
DFT_float   i_out_dft = DFT_FLOAT_DEFAULTS;
float	i_out_rms = 0.0;
float	i_out_form = 0.0;
float	i_out_kot = 0.0;
float   i_out_kot_old = 0.0;

// prvi harmonik in RMS izhodne napetosti (u_out)
DFT_float   u_out_dft = DFT_FLOAT_DEFAULTS;
float   u_out_rms_ref = U_AC_RMS_REF;
float   u_out_rms = 0.0;
float   u_out_form = 0.0;

float   u_out_err = 0.0;

// regulacija napetosti enosmernega tokokroga
PID_float   u_dc_reg = PID_FLOAT_DEFAULTS;
SLEW_float  u_dc_slew = SLEW_FLOAT_DEFAULTS;

// regulacija omreznega toka
PID_float   i_s_reg = PID_FLOAT_DEFAULTS;

// regulacija izhodne napetosti
PID_float	u_out_PIreg = PID_FLOAT_DEFAULTS;
PID_float	u_out_DC_PIreg = PID_FLOAT_DEFAULTS;
REP_float	u_out_RepReg = REP_FLOAT_DEFAULTS;

float		u_out_RepReg_k_in = 1.0/DEL_UDC_REF;

float		u_out_duty = 0.0;		// kar posiljam na FB2

// sinhronizacija na omrežje
float       sync_base_freq = SAMPLE_FREQ;
PID_float   sync_reg    = PID_FLOAT_DEFAULTS;
float       sync_switch_freq = SAMPLE_FREQ;
float       sync_grid_freq = (SAMPLE_FREQ/SAMPLE_POINTS);
bool        sync_use = TRUE;

// samo za statistiko meritev
STAT_float  statistika = STAT_FLOAT_DEFAULTS;

// za oceno DC-link toka
ABF_float   i_cap_dc = ABF_FLOAT_DEFAULTS;
float       i_dc_abf = 0.0;

// za zakasnitev omreznega toka
DELAY_float i_s_delay = DELAY_FLOAT_DEFAULTS;

// filtriranje izhoda ocene
DC_float    i_dc_f = DC_FLOAT_DEFAULTS;

// filtriranje meritve
DC_float    i_f_f = DC_FLOAT_DEFAULTS;
DC_float	i_s_f = DC_FLOAT_DEFAULTS;
DC_float	i_out_f = DC_FLOAT_DEFAULTS;
DC_float	u_f_f = DC_FLOAT_DEFAULTS;
DC_float	u_out_f = DC_FLOAT_DEFAULTS;
DC_float	u_ac_f = DC_FLOAT_DEFAULTS;

// koeficient za sprotno kalibracijo offsetov
float	k_offset_u_ac = 1.0e-4;
float	k_offset_u_out = 1.0e-4;
float	k_offset_u_f = 5.0e-3;
float	k_offset_IF = 5.0e-3;

// vhodna moc
float	power_in = 0.0;

// izhodna moc
float   power_out = 0.0;

// temperatura hladilnika
float   temperatura = 0.0;

// prototipi funkcij
void get_electrical(void);
void sync(void);
void input_bridge_control(void);
void output_bridge_enable(void);
void output_bridge_disable(void);
void output_bridge_control(void);
void check_limits(void);

// spremenljikva s katero štejemo kolikokrat se je prekinitev predolgo izvajala
int interrupt_overflow_counter = 0;

/**************************************************************
* Prekinitev, ki v kateri se izvaja regulacija
**************************************************************/
#pragma CODE_SECTION(PER_int, "ramfuncs");
void interrupt PER_int(void)
{
    /* lokalne spremenljivke */
    
    // najprej povem da sem se odzzval na prekinitev
    // Spustimo INT zastavico casovnika ePWM1
    EPwm1Regs.ETCLR.bit.INT = 1;
    // Spustimo INT zastavico v PIE enoti
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
    
    // pozenem stoparico
    interrupt_cycles = TIC_time;
    TIC_start();

    // izracunam obremenjenost procesorja
    cpu_load = (float)interrupt_cycles / (CPU_FREQ/SAMPLE_FREQ);

    // povecam stevec prekinitev
    interrupt_cnt = interrupt_cnt + 1;
    if (interrupt_cnt >= SAMPLE_FREQ)
    {
        interrupt_cnt = 0;
    }

    // generiram zeljeno vrednost
    REF_GEN_update();

    // izracun napetosti, tokov in temperature hladilnika
    get_electrical();

    // preverim ali sem znotraj meja
    check_limits();

    // sinhronizacija na omrezje
    sync();

    // regulacija u_dc
    input_bridge_control();

    // za zagon in izklop filtra
    output_bridge_enable();
    output_bridge_disable();

    // regulacija u_out
    output_bridge_control();

    // spravim vrednosti v buffer za prikaz
    DLOG_GEN_update();
    
    /* preverim, èe me sluèajno èaka nova prekinitev.
       èe je temu tako, potem je nekaj hudo narobe
       saj je èas izvajanja prekinitve predolg
       vse skupaj se mora zgoditi najmanj 10krat,
       da reèemo da je to res problem
    */
    if (EPwm1Regs.ETFLG.bit.INT == TRUE)
    {
        // povecam stevec, ki steje take dogodke
        interrupt_overflow_counter = interrupt_overflow_counter + 1;
        
        // in ce se je vse skupaj zgodilo 10 krat se ustavim
        // v kolikor uC krmili kakšen resen HW, potem moèno
        // proporoèam lepše "hendlanje" takega dogodka
        // beri:ugasni moènostno stopnjo, ...
        if (interrupt_overflow_counter >= 10)
        {
        	// izklopim mostic
        	FB1_disable();
        	FB2_disable();

        	// izklopim 5V_ISO linijo
        	PCB_5V_ISO_off();

        	// izklopim vse kontaktorje
        	PCB_relay1_off();
        	PCB_relay2_off();
        	PCB_relay3_off();

            asm(" ESTOP0");
        }
    }
    
    // stopam
    TIC_stop();

	PCB_WD_KICK_int();

}   // end of PWM_int

/**************************************************************
* Funckija, ki pripravi vse potrebno za izvajanje
* prekinitvene rutine
**************************************************************/
void PER_int_setup(void)
{
    // inicializiram data logger
    dlog.mode = Single;
    dlog.auto_time = 1;
    dlog.holdoff_time = 1;

    dlog.prescalar = 1;

    dlog.slope = Positive;
    dlog.trig = &u_out;
    dlog.trig_value = 0.0;

    dlog.iptr1 = &u_ac;
    dlog.iptr2 = &u_out;
    dlog.iptr3 = &u_out_err;
    dlog.iptr4 = &i_out;
    dlog.iptr5 = &u_out_duty;
    dlog.iptr6 = &i_f;
    dlog.iptr7 = &u_f;
    dlog.iptr8 = &u_dc;

    // inicializiram generator signalov
    ref_gen.type = REF_Step;
    ref_gen.amp = 1.0;
    ref_gen.offset = 0.0;
    ref_gen.freq = 10.0;
    ref_gen.duty = 0.5;
    ref_gen.slew = 100;
    ref_gen.samp_period = SAMPLE_TIME;

    // inicializiram DC filter
    DC_FLOAT_MACRO_INIT(u_dc_f);

    // inicilaliziram DFT
    DFT_FLOAT_MACRO_INIT(u_ac_dft);
    DFT_FLOAT_MACRO_INIT(u_out_dft);
    DFT_FLOAT_MACRO_INIT(i_out_dft);

    // inicializiram u_dc_slew
    u_dc_slew.In = DEL_UDC_REF;					// referenca u_dc = 40 V
    u_dc_slew.Slope_up = 10.0 * SAMPLE_TIME;	// 10 V/s
    u_dc_slew.Slope_down = u_dc_slew.Slope_up;

    // inicializacija PI regulator DC_link napetosti (empiricno doloceni parametri)
    u_dc_reg.Kp = 2.2;
    u_dc_reg.Ki = 0.0002;
    u_dc_reg.Kff = 0.8;
    u_dc_reg.OutMax = 5.0;			// IS_max 5.0 A
    u_dc_reg.OutMin = -5.0;		// IS_min -5.0 A

    // inicializacija PI regulator omreznega toka IS (Optimum iznosa)
    i_s_reg.Kp = 0.1885;
    i_s_reg.Ki = 0.004642;
    i_s_reg.Kff = 1.0;
    i_s_reg.OutMax = +0.99;    // zaradi bootstrap driverjev ne gre do 1.0
    i_s_reg.OutMin = -0.99;    // zaradi bootstrap driverjev ne gre do 1.0

    // regulator frekvence
    sync_reg.Kp = 1000;
    sync_reg.Ki = 0.01;
    sync_reg.OutMax = +SWITCH_FREQ/10;
    sync_reg.OutMin = -SWITCH_FREQ/10;

    // inicializacija PI regulator u_out_DC
    u_out_DC_PIreg.Kp = 0.0;
    u_out_DC_PIreg.Ki = -0.0001;
    u_out_DC_PIreg.Kff = 0.0;
    u_out_DC_PIreg.OutMax = +0.99;		// zaradi bootstrap driverjev ne gre do 1.0
    u_out_DC_PIreg.OutMin = -0.99;		// zaradi bootstrap driverjev ne gre do 1.0

    // inicializacija PI regulator u_out
    u_out_PIreg.Kp = 0.001;
    u_out_PIreg.Ki = 0.0025;
    u_out_PIreg.Kff = 0.0;
    u_out_PIreg.OutMax = +0.99;		// zaradi bootstrap driverjev ne gre do 1.0
    u_out_PIreg.OutMin = -0.99;		// zaradi bootstrap driverjev ne gre do 1.0

    // inicializacija repetitivni regulator u_out
    u_out_RepReg.gain = 0.04;
    u_out_RepReg.w0 = 0.5;
    u_out_RepReg.w1 = 0.250;
    u_out_RepReg.w2 = 0.0;
    u_out_RepReg.OutMax = 0.99;		// zaradi bootstrap driverjev ne gre do 1.0
    u_out_RepReg.OutMin = -0.99;	// zaradi bootstrap driverjev ne gre do 1.0
    u_out_RepReg.delay_komp = 2;


    // inicializiram buffer za rep. reg.
    REP_float_init(&u_out_RepReg);

    // inicializiram statistiko
    STAT_FLOAT_MACRO_INIT(statistika);

    // inicializiram ABF
                                    // 2000 Hz;     1000 Hz;     500 Hz,      100 Hz          50 Hz           10 Hz
    i_cap_dc.Alpha = 0.043935349;  	// 0.6911845;   0.394940272 ; 0.209807141; 0.043935349;    0.022091045;    0.004437948
    i_cap_dc.Beta = 0.00098696;   	// 0.394784176; 0.098696044; 0.024674011; 0.00098696;     0.00024674;     0.0000098696
    i_cap_dc.Capacitance = 5 * 0.0022;   	// 5 * 2200 uF

    // inicializiram delay_linijo
    DELAY_FLOAT_INIT(i_s_delay)
    i_s_delay.delay = 10;

    // inicializiram filter za oceno toka
    DC_FLOAT_MACRO_INIT(i_dc_f);

    // inicializiram filter za meritev toka
    DC_FLOAT_MACRO_INIT(i_f_f);

    // inicializiram filter za meritev toka
    DC_FLOAT_MACRO_INIT(i_out_f);

    // inicializiram filter za u_f
    DC_FLOAT_MACRO_INIT(u_f_f);

    // incializiram filter za u_out
    DC_FLOAT_MACRO_INIT(u_out_f);

    // incializiram filter za u_ac
    DC_FLOAT_MACRO_INIT(u_ac_f);

    // inicializiram štoparico
    TIC_init();

    // Proženje prekinitve
    EPwm1Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;    //sproži prekinitev na periodo
    EPwm1Regs.ETPS.bit.INTPRD = ET_1ST;         //ob vsakem prvem dogodku
    EPwm1Regs.ETCLR.bit.INT = 1;                //clear possible flag
    EPwm1Regs.ETSEL.bit.INTEN = 1;              //enable interrupt

    // registriram prekinitveno rutino
    EALLOW;
    PieVectTable.EPWM1_INT = &PER_int;
    EDIS;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
    PieCtrlRegs.PIEIER3.bit.INTx1 = 1;
    IER |= M_INT3;
    // da mi prekinitev teèe  tudi v real time naèinu
    // (za razhoršèevanje main zanke in BACK_loop zanke)
    SetDBGIER(M_INT3);
}

#pragma CODE_SECTION(get_electrical, "ramfuncs");
void get_electrical(void)
{

    static float   	IS_offset_calib = 0.0;
    static float   	IF_offset_calib = 0.0;
    static float   	I_out_offset_calib = 0.0;
    static float	u_f_offset_calib = 0.0;

    // pocakam da ADC konca s pretvorbo
    ADC_wait();
    // poberem vrednosti iz AD pretvornika

    // zacetna kalibracija offseta za IS, IF, u_f
    if (   (start_calibration == TRUE)
        && (calibration_done == FALSE))
    {
        // akumuliram offset
        IS_offset_calib = IS_offset_calib + IS_adc;
        IF_offset_calib = IF_offset_calib + IF_adc;
        I_out_offset_calib = I_out_offset_calib + I_out_adc;
        u_f_offset_calib = u_f_offset_calib + u_f_adc;

        // ko potece dovolj casa, sporocim da lahko grem naprej
        // in izracunam povprecni offset
        current_offset_counter = current_offset_counter + 1;
        if (current_offset_counter == (SAMPLE_FREQ * 1L))
        {
            calibration_done = TRUE;
            start_calibration = FALSE;
            i_s_offset = IS_offset_calib / (SAMPLE_FREQ*1L);
            i_f_offset = IF_offset_calib / (SAMPLE_FREQ*1L);
            i_out_offset = I_out_offset_calib / (SAMPLE_FREQ*1L);
            u_f_offset = u_f_offset_calib / (SAMPLE_FREQ*1L);

        }

        i_s = 0.0;
        i_f = 0.0;
        i_out = 0.0;
        u_ac = 0.0;
        u_dc = 0.0;
        u_f = 0.0;
        u_out = 0.0;
    }
    else
    {

    	// sprotna kalibracija v rezimu Standby
    	if (state == Standby)
    	{
    		// kalibriram offset
    		u_ac_offset = u_ac_offset - k_offset_u_ac * u_ac_f.Mean;
    		u_out_offset = u_out_offset + k_offset_u_out * u_out_f.Mean;
    		u_f_offset = u_f_offset - k_offset_u_f * u_f_f.Mean;
    		i_f_offset = i_f_offset + k_offset_IF * i_f_f.Mean;
    	}

    	if (state == Working)
    	{
    		u_ac_offset = u_ac_offset - k_offset_u_ac * u_ac_f.Mean;
    	}

        i_s = i_s_gain * (IS_adc - i_s_offset);
        i_f = i_f_gain * (IF_adc - i_f_offset);
        i_out = i_out_gain * (I_out_adc - i_out_offset);
        u_ac = u_ac_gain * (u_ac_adc - u_ac_offset);
        u_dc = u_dc_gain * (DEL_UDC_adc - u_dc_offset);
        u_f = u_f_gain * (u_f_adc - u_f_offset);
        u_out = u_out_gain * (u_out_adc - u_out_offset);
    }

    // poracunam DFT napetosti
    // vhodna omrežna napetost - u_ac
    u_ac_dft.In = u_ac;
    DFT_FLOAT_MACRO(u_ac_dft);

    // naraèunam RMS omrežne napetosti - u_ac
    u_ac_rms = ZSQRT2 * sqrt(u_ac_dft.SumA * u_ac_dft.SumA + u_ac_dft.SumB *u_ac_dft.SumB);

    // normiram, da dobim obliko
    u_ac_form = u_ac_dft.Out / (u_ac_rms * SQRT2);

    // filtriram meritev u_ac
    u_ac_f.In = u_ac;
    DC_FLOAT_MACRO(u_ac_f);

    // izracunam trenutni kot omrezja
    u_ac_kot = (u_ac_dft.kot - PI) / PI;

    // generiram zeljeno vrednost za u_out (230V RMS)
    u_ac_zeljena = GRID_AMPLITUDE * u_ac_form ;

    // izhodna napetost - u_out
    u_out_dft.In = u_out;
    DFT_FLOAT_MACRO(u_out_dft);

    // naraèunam amplitudo izhodne napetosti - u_out
    u_out_rms = ZSQRT2 * sqrt(u_out_dft.SumA * u_out_dft.SumA + u_out_dft.SumB *u_out_dft.SumB);

    // naracunam napako
    // ce ne popravljam amplitude, potem je napaka kar direktna razlika med osnovnim harmonikom
    // in merjeno napetostjo
    if (amp_control == FALSE)
    {
        u_out_err = (u_ac_rms / u_out_rms) * u_out_dft.Out - u_out;
    }
    // sicer pa ustrezno skaliram osnovni harmonik
    else
    {
        u_out_err = (u_out_rms_ref / u_out_rms) * u_out_dft.Out - u_out;
    }

    // normiram, da dobim obliko
    u_out_form = u_out_dft.Out / (u_out_rms * SQRT2);

    // filtriram meritev u_out
    u_out_f.In = u_out;
    DC_FLOAT_MACRO(u_out_f);

    // filtriram meritev toka IF
    i_f_f.In = i_f;
    DC_FLOAT_MACRO(i_f_f);

    // filtriram meritev toka I_out
    i_out_f.In = i_out;
    DC_FLOAT_MACRO(i_out_f);

    // poracunam DFT I_out
    i_out_dft.In = i_out;
    DFT_FLOAT_MACRO(i_out_dft);

    // naraèunam RMS omrežne napetosti - u_ac
    i_out_rms = ZSQRT2 * sqrt(i_out_dft.SumA * i_out_dft.SumA + i_out_dft.SumB * i_out_dft.SumB);

    // normiram, da dobim obliko
    i_out_form = i_out_dft.Out / (i_out_rms * SQRT2);

    // izracunam trenutni kot I_out, shranim staro vrednost
    i_out_kot_old = i_out_kot;
    i_out_kot = 2 * asin(i_out_form) / PI;

    // filtriram meritev u_f
    u_f_f.In = u_f;
    DC_FLOAT_MACRO(u_f_f);

    // filtriram DC link napetost
    u_dc_f.In = u_dc;
    DC_FLOAT_MACRO(u_dc_f);
    u_dc_filtered = u_dc_f.Mean;

    // izracunam kaksna moc je na izhodu in vhodu filtra
    power_out = u_f * i_f;
    power_in = (24.0 / 230.0) * u_ac * i_s;

    // zakasnim IS
    i_s_delay.in = i_s * i_s_reg.Out;
    DELAY_FLOAT_CALC(i_s_delay);

    // ocena dc toka z ABF
    i_cap_dc.u_cap_measured = u_dc;
    ABF_float_calc(&i_cap_dc);

    // se filtriram
    i_dc_f.In = -i_cap_dc.i_cap_estimated - i_s_delay.out;
    DC_FLOAT_MACRO(i_dc_f);
    i_dc_abf = i_dc_f.Mean;

    // statistika
    statistika.In = u_dc;
    STAT_FLOAT_MACRO(statistika);
}

#pragma CODE_SECTION(sync, "ramfuncs");
void sync(void)
{
	// sinhronizacija na omrezje
    sync_reg.Ref = 0;
    sync_reg.Fdb = u_ac_dft.SumA/u_ac_rms;
    PID_FLOAT_CALC(sync_reg);

    sync_switch_freq = sync_base_freq + sync_reg.Out;

    sync_grid_freq = sync_switch_freq/SAMPLE_POINTS;

    if (sync_use == TRUE)
    {
        FB1_frequency(sync_switch_freq);
        FB2_frequency(sync_switch_freq);
    }
    else
    {
        FB1_frequency(sync_base_freq);
        FB2_frequency(sync_base_freq);
    }
}

#pragma CODE_SECTION(input_bridge_control, "ramfuncs");
void input_bridge_control(void)
{
    // regulacija deluje samo v teh primerih
    if (   (state == Charging)
        || (state == Standby)
        || (state == Enable)
        || (state == Working)
        || (state == Disable))
    {
        // najprej zapeljem zeljeno vrednost po rampi
        SLEW_FLOAT_CALC(u_dc_slew)

        // napetostni PI regulator
        u_dc_reg.Ref = u_dc_slew.Out;
        u_dc_reg.Fdb = u_dc_filtered;
        // uporabim ABF za oceno DC toka in posledièno feedforward
        u_dc_reg.Ff = i_dc_abf * (230 / 24) * u_dc_filtered * SQRT2 / u_ac_rms;

        PID_FLOAT_CALC(u_dc_reg);

        // tokovni PI regulator s feed forward za IS
        i_s_reg.Ref = u_dc_reg.Out * u_ac_form;
        i_s_reg.Fdb = i_s;
        i_s_reg.Ff = -(24.0 / 230.0) * u_ac/u_dc;
        PID_FLOAT_CALC(i_s_reg);

        // posljem vse skupaj na mostic
        FB1_update(i_s_reg.Out);
    }
    // sicer pa nicim integralna stanja
    else
    {
        u_dc_reg.Ui = 0.0;
        i_s_reg.Ui = 0.0;
        FB1_update(0.0);
    }
}

#pragma CODE_SECTION(output_bridge_enable, "ramfuncs");
void output_bridge_enable(void)
{
	// delujemo samo v primeru vklopa izhodnega mostica, ko je rele 3 ze odklopljen
	if (	(state == Enable)
		&&	(enable == TRUE)
		&&	(PCB_CPLD_MOSFET_MCU_status() == TRUE)
		&&	(PCB_relay3_status() == TRUE)
		&&	(FB2_status() == FB_EN)				)
	{
		// detekcija prehoda toka skozi 0
		if (	(i_out_kot_old < 0.0)
			&&	(i_out_kot > 0.0)	)
		{
			PCB_CPLD_MOSFET_MCU_off();

			// pobrisem zastavico
			enable = FALSE;

			state = Working;
		}
	}
}

#pragma CODE_SECTION(output_bridge_enable, "ramfuncs");
void output_bridge_disable(void)
{
	// zacetek izklopne rutine, pri kotu 0°
		if (	(state == Disable)
			&&	(PCB_CPLD_MOSFET_MCU_status() == FALSE)
			&&	(PCB_relay3_status() == TRUE)
			&&	(FB2_status() == FB_EN)					)
		{
			// detekcija prehoda toka skozi 0
			if (	(i_out_kot <= 0.005)
				&&	(i_out_kot >= -0.005)	)
			{
				PCB_CPLD_MOSFET_MCU_on();
				PCB_relay3_off();
				FB2_disable();

				// postavim zastavico za nadaljo izklopno rutino v Back_loop
				disable = TRUE;
			}
		}
}

#pragma CODE_SECTION(output_bridge_control, "ramfuncs");
void output_bridge_control(void)
{
    // regulacija deluje samo v tem primeru
    if 	(	(state == Working)
    	||	(state == Disable)	)
    {
    	// detekcija, ce delujemo v rezimu regulacije
    	if (mode == Control)
    	{
    		// PI regulator za odpravo DC offseta toka
    		u_out_DC_PIreg.Ref = 0;
    		if (dc_control == None)
    		{
    		    u_out_DC_PIreg.Ui = 0.0;
	            u_out_DC_PIreg.Out = 0.0;
    		}
    		if (dc_control == Voltage)
    		{
    		    u_out_DC_PIreg.Fdb = u_out_f.Mean;
                u_out_DC_PIreg.Ff = 0.0;
                PID_FLOAT_CALC(u_out_DC_PIreg);
    		}
    		if (dc_control == Current)
    		{
    		    u_out_DC_PIreg.Fdb = i_f_f.Mean;
                u_out_DC_PIreg.Ff = 0.0;
                PID_FLOAT_CALC(u_out_DC_PIreg);
    		}


    		// izbira vrste regulacije izhodne napetosti poleg osnovnega PI regulatorja
    		switch (out_control)
    		{
    		case PI_ONLY:
    		    // nicim repetitivni regulator
    		    REP_float_zero(&u_out_RepReg);

                // PI regulator
                u_out_PIreg.Ref = 0.0;
                u_out_PIreg.Fdb = u_out_err;
                u_out_PIreg.Ff = 0.0;
                PID_FLOAT_CALC(u_out_PIreg);

                // duty za izhodni mostic
                u_out_duty = u_out_DC_PIreg.Out + u_out_PIreg.Out;

    		    break;

    		case PI_REP:
                // PI regulator
                u_out_PIreg.Ref = 0.0;
                u_out_PIreg.Fdb = u_out_err;
                u_out_PIreg.Ff = 0.0;
                PID_FLOAT_CALC(u_out_PIreg);

                // repetitivni regulator
                u_out_RepReg.in = u_out_RepReg_k_in * (0.0-u_out_err);
                REP_float_calc(&u_out_RepReg);

                // duty za izhodni mostic
                u_out_duty = u_out_DC_PIreg.Out + u_out_PIreg.Out + u_out_RepReg.out;

    		    break;

    		case REP:
    		    // nicim PI regulator
    		    u_out_PIreg.Ui = 0.0;

                // repetitivni regulator
                u_out_RepReg.in = u_out_RepReg_k_in * (0.0-u_out_err);
                REP_float_calc(&u_out_RepReg);

                // duty za izhodni mostic
                u_out_duty = u_out_DC_PIreg.Out + u_out_RepReg.out;

    			break;

    		case RES:
    			// resonanèni regulator
    		    u_out_duty = u_out_DC_PIreg.Out;
    			break;

    		default:
    		    u_out_duty = 0.0;
    		}

    		// omejim izhod (limita: -0.99 - +0.99)
    		if(u_out_duty >= 1.0)
    		{
    			u_out_duty = 0.99;
    		}

    		if(u_out_duty <= -1.0)
    		{
    			u_out_duty = -0.99;
    		}
    		// posljem vse skupaj na mostic
    		FB2_update(u_out_duty);
    	}
    	else
    	{
    		// krmiljenje (brez regulacije)
    		FB2_update(u_ac_form * i_f_zeljen);
    	}
    }
    // sicer pa nicim integralna stanja
    else
    {
        FB2_update(0.0);
        u_out_PIreg.Ui = 0.0;
        u_out_DC_PIreg.Ui = 0.0;

        u_out_PIreg.Out = 0.0;
        u_out_DC_PIreg.Out = 0.0;
        u_out_RepReg.out = 0.0;

        REP_float_zero(&u_out_RepReg);
    }
}

#pragma CODE_SECTION(check_limits, "ramfuncs");
void check_limits(void)
{
    // samo èe je kalibracija konènana
    if (calibration_done == TRUE)
    {
         if (u_ac_rms > U_AC_RMS_MAX)
        {
            fault_flags.overvoltage_u_ac = TRUE;
            state = Fault_sensed;
            // izklopim mostic
            FB1_disable();
            FB2_disable();

            // izklopim 5V_ISO linijo
            PCB_5V_ISO_off();

            // izklopim vse kontaktorje
            PCB_relay1_off();
            PCB_relay2_off();
            PCB_relay3_off();
        }

        if (   (u_ac_rms < U_AC_RMS_MIN)
                && (state != Initialization)
                && (state != Startup))
        {
            fault_flags.undervoltage_u_ac = TRUE;
            state = Fault_sensed;
            // izklopim mostic
            FB1_disable();
            FB2_disable();

            // izklopim 5V_ISO linijo
            PCB_5V_ISO_off();

            // izklopim vse kontaktorje
            PCB_relay1_off();
            PCB_relay2_off();
            PCB_relay3_off();
        }

        if (	(fabs(u_f) > U_F_LIM)
        	&&	(state == Working)	)
        {
        	fault_flags.overvoltage_u_f = TRUE;
        	state = Fault_sensed;
        	// izklopim mostic
        	FB1_disable();
        	FB2_disable();

        	// izklopim 5V_ISO linijo
        	PCB_5V_ISO_off();

        	// izklopim vse kontaktorje
        	PCB_relay1_off();
        	PCB_relay2_off();
        	PCB_relay3_off();
        }

        if (u_dc > DEL_UDC_MAX)
        {
            fault_flags.overvoltage_u_dc = TRUE;
            state = Fault_sensed;
            // izklopim mostic
            FB1_disable();
            FB2_disable();

            // izklopim 5V_ISO linijo
            PCB_5V_ISO_off();

            // izklopim vse kontaktorje
            PCB_relay1_off();
            PCB_relay2_off();
            PCB_relay3_off();
        }

        if (   (u_dc < DEL_UDC_MIN)
                && (state != Initialization)
                && (state != Startup))
        {
            fault_flags.undervoltage_u_dc = TRUE;
            state = Fault_sensed;
            // izklopim mostic
            FB1_disable();
            FB2_disable();

            // izklopim 5V_ISO linijo
            PCB_5V_ISO_off();

            // izklopim vse kontaktorje
            PCB_relay1_off();
            PCB_relay2_off();
            PCB_relay3_off();
        }

        if ((i_s > +IS_LIM) || (i_s < -IS_LIM))
        {
        	fault_flags.overcurrent_IS = TRUE;
        	state = Fault_sensed;
        	// izklopim mostic
        	FB1_disable();
        	FB2_disable();

        	// izklopim 5V_ISO linijo
        	PCB_5V_ISO_off();

       		// izklopim vse kontaktorje
       		PCB_relay1_off();
       		PCB_relay2_off();
       		PCB_relay3_off();
        }

        if ((i_f > +IF_LIM) || (i_f < -IF_LIM))
        {
            fault_flags.overcurrent_IF = TRUE;
            state = Fault_sensed;
            // izklopim mostic
            FB1_disable();
            FB2_disable();

            // izklopim 5V_ISO linijo
            PCB_5V_ISO_off();

            // izklopim vse kontaktorje
            PCB_relay1_off();
            PCB_relay2_off();
            PCB_relay3_off();
        }

    }
}
