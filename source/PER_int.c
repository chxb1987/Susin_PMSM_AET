/****************************************************************
* FILENAME:     PER_int.c
* DESCRIPTION:  periodic interrupt code
* AUTHOR:       Mitja Nemec, Denis Susin
*
****************************************************************/
#include    "PER_int.h"
#include    "TIC_toc.h"


// CPU load estimation
float   cpu_load  = 0.0;
long    interrupt_cycles = 0;

// CPU temperature
float	cpu_temp = 0.0;

// counter of too long interrupt function executions
int     interrupt_overflow_counter = 0;

// mechanical variables
int		kot_raw = 0;
float	kot_meh_ref = 0.0;
float	kot_meh = 0.0;
float	kot_el = 0.0;
float	speed_meh_ref = 0.0;
float	speed_meh = 0.0;
float	accel_meh_Hz_per_s = 0.0;
ABF_omega	abf_speed_meh = ABF_OMEGA_DEFAULTS;
ABF_omega	abf_accel_meh = ABF_OMEGA_DEFAULTS;

// current variables
long 	current_offset_counter = 0;

long   	tok_i1_raw_accu = 0;
long   	tok_i2_raw_accu = 0;
long   	tok_i3_raw_accu = 0;

long	tok_i1_raw_offset = 0;
long	tok_i2_raw_offset = 0;
long	tok_i3_raw_offset = 0;

float   tok_i_gain = (48.0/0.625) * (7.5/5.6) * (1.0/4096.0);// (7.5/5.6) * (48.0/0.625) * (3.0/4096.0) * (1.0/3.0);

float	tok_i1 = 0.0;
float	tok_i2 = 0.0;
float	tok_i3 = 0.0;

float	tok_d_ref = 0.0;
float	tok_q_ref = 0.0;
float	tok_d = 0.0;
float	tok_q = 0.0;

CLARKE_float 	clarke_tok = CLARKE_FLOAT_DEFAULTS;
PARK_float 		park_tok = PARK_FLOAT_DEFAULTS;

// voltage variables
float	nap_v1_offset = 0.0;
float	nap_v2_offset = 0.0;
float	nap_v3_offset = 0.0;
float	nap_dc_offset = 0.0;

float   nap_v_gain = 0.982 * ((620.0 + 10000.0)/620.0) * (3.0/4096.0);
float	nap_dc_gain = 0.971 * ((620.0 + 10000.0)/620.0) * (3.0/4096.0);

float	nap_v1 = 0.0;
float	nap_v2 = 0.0;
float	nap_v3 = 0.0;
float	nap_dc = 0.0;

float	nap_alpha_ref = 0.0;
float	nap_beta_ref = 0.0;
float	nap_d_ref = 0.0;
float	nap_q_ref = 0.0;

// other electrical variables
float	pot_rel = 0.0;
float	pot_rel_discrete = 0.0;
float	pot_rel_discrete_old = 0.0;

/* control algorithm variables */
// general variables
float	duty_DC = 0.0;
float 	duty_six_step = 0.0;
int 	sector_six_step = 1;
float	amp_rel = 0.0;
float	freq = 0.0;
float	freq_meh = 0.0;

int		direction = 1;
long	tic_direction = 0;
long	delta_tic_direction = 0;

volatile enum	MODULATION modulation = SVM;
volatile enum	CONTROL control = OPEN_LOOP;

IPARK_float		ipark_nap = IPARK_FLOAT_DEFAULTS;

// controllers variables
PI_ctrl			id_reg = PI_CTRL_DEFAULTS;
PI_ctrl			iq_reg = PI_CTRL_DEFAULTS;
PI_ctrl			speed_reg = PI_CTRL_DEFAULTS;
PID_ctrl		position_reg = PI_CTRL_DEFAULTS;

// PI regulator toka
// upo�tevano: NORMA_I = 50.0
float   Kp_id_reg = 0.03;    	  		// Vdc = 12V: 0.03            	Vdc = 24V: 0.015
float   Ki_id_reg = 26.0/SAMPLE_FREQ;	// Vdc = 12V: 26.0/SAMPLE_FREQ 	Vdc = 24V: 13.0/SAMPLE_FREQ
float   Kp_iq_reg = 0.03;    			// Vdc = 12V: 0.03            	Vdc = 24V: 0.015
float   Ki_iq_reg = 26.0/SAMPLE_FREQ;   // Vdc = 12V: 26.0/SAMPLE_FREQ 	Vdc = 24V: 13.0/SAMPLE_FREQ

// PI regulator hitrosti
float   Kp_speed_reg = 3.0;  			// velja �e merimo napetost z ABF: Kp = 3.0
float   Ki_speed_reg = 5e-4;  			// velja �e merimo napetost z ABF: Ki = 5e-4

// PID regulator pozicije
float   Kp_position_reg = 200.0;  		// velja, �e ni hitrostne zanke Kp = 200.0
float   Ki_position_reg = 0.0;  		// velja, �e ni hitrostne zanke Ki = 0.0
float   Kd_position_reg = 10.0;  		// velja, �e ni hitrostne zanke Kd = 10.0

// software limits
float	nap_dc_max = 50.0; 						// V
float	nap_dc_min = 0.0;						// V
float	nap_v_max = 50.0;						// V
float	nap_v_min = -50.0;						// V
float	tok_i_max = 45.0;						// A

float   nap_d_ref_max = 0.577350269189626; 		// per unit
float   nap_d_ref_min = -0.577350269189626;   	// per unit
float   nap_q_ref_max = 0.577350269189626;    	// per unit
float   nap_q_ref_min = -0.577350269189626;   	// per unit
float   tok_d_ref_max = 5.0;   					// A
float   tok_d_ref_min = -5.0;  					// A
float   tok_q_ref_max = 44.0;   				// A
float   tok_q_ref_min = -44.0;  				// A

float   navor_ref_max = 5.89;    				// Nm
float   navor_ref_min = -5.89;   				// Nm

float   speed_ref_max = 30.0;  					// Hz
float   speed_ref_min = -30.0; 					// Hz

// flags
bool 	current_offset_calibrated_flag = FALSE;

bool	direction_change_flag = FALSE;

bool	set_null_position_flag = FALSE;
bool	reset_null_position_procedure_flag = FALSE;
bool	control_enable_flag = FALSE;

bool 	incremental_encoder_connected_flag = FALSE;

bool 	hardware_trip_oc_flag = FALSE;
bool	software_trip_flag = FALSE;
bool	trip_reset_flag = FALSE;

bool	nap_dc_overvoltage_flag = FALSE;
bool	nap_dc_undervoltage_flag = FALSE;
bool	nap_v1_overvoltage_flag = FALSE;
bool	nap_v1_undervoltage_flag = FALSE;
bool	nap_v2_overvoltage_flag = FALSE;
bool	nap_v2_undervoltage_flag = FALSE;
bool	nap_v3_overvoltage_flag = FALSE;
bool	nap_v3_undervoltage_flag = FALSE;
bool	tok_i1_overcurrent_flag = FALSE;
bool	tok_i2_overcurrent_flag = FALSE;
bool	tok_i3_overcurrent_flag = FALSE;

// temporary variables
float 	temp1 = 0.0;
float 	temp2 = 0.0;
float 	temp3 = 0.0;

// extern variables
extern bool 	sw1_state;
extern bool 	b1_state;
extern bool 	b1_press_int;
extern bool 	b2_press_int;
extern bool 	b3_press_int;
extern bool 	b4_press_int;
extern enum 	SVM_STATE svm_status;

// functions
void 	get_mechanical(void);
void 	get_meh_speed(void);
void 	get_meh_accel(void);
void 	get_electrical(void);
void 	set_null_position(bool reset_procedure);
void 	software_protection(void);
void	control_algorithm(void);
void	open_loop_control(void);
void	current_loop_control(void);
void	speed_loop_control(void);
void	position_loop_control(void);
void	trip_reset(void);

/**************************************************************
* interrupt funcion
**************************************************************/
#pragma CODE_SECTION(PER_int, "ramfuncs");
void interrupt PER_int(void)
{
    /// local variables
    
    // acknowledge interrupt within PWM module
    EPwm1Regs.ETCLR.bit.INT = 1;
    // acknowledge interrupt within PIE module
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
    
    // start CPU load stopwatch
    interrupt_cycles = TIC_time;
    TIC_start();

    // get previoust CPU load estimate
    cpu_load = (float)interrupt_cycles / (CPU_FREQ/SAMPLE_FREQ);

    // increase and wrap around interrupt counter every 1 second
    interrupt_cnt = interrupt_cnt + 1;
    if (interrupt_cnt >= SAMPLE_FREQ)
    {
        interrupt_cnt = 0;
        interrupt_cnt_s = interrupt_cnt_s + 1;
    }

    // number of seconds passed
    if (interrupt_cnt_s == 60)
    {
    	interrupt_cnt_s = 0;
    	interrupt_cnt_min = interrupt_cnt_min + 1;
    }


    // reference value generator
    REF_GEN_update();

    // wait for the ADC to finish with conversion
    ADC_A_wait();
    ADC_B_wait();
    ADC_C_wait();

    // calculate CPU temperature
    cpu_temp = GetTemperatureC(ADC_TEMP);

    // read mechanical (rotor angle) and electrical signals (currents, voltages, pots, ...)
    get_mechanical();
    get_electrical();




    /* 3 phase inverter control alghorithm */




    // buttons
    if(control_enable_flag == FALSE)
    {
    	// button 2 changes direction of rotation
    	if(b2_press_int == TRUE)
    	{
    		direction = -direction;
    		if(direction >= 0)
    		{
    			direction = 1;
    		}
    		else
    		{
    			direction = -1;
    		}

    		direction_change_flag = TRUE;
        	tic_direction = interrupt_cnt;
    	}
    	// button 3 changes modulation mode
    	if(b3_press_int == TRUE)
    	{
    		modulation = modulation + 1;
    	}
    	if(modulation == 3)
    	{
    		modulation = 0;
    	}

    	// button 4 changes control mode
    	if(b4_press_int == TRUE)
    	{
    		control = control + 1;
    	}
    	if(control == 4)
    	{
    		control = 0;
    	}

    	if(modulation != SVM)
    	{
    		control = OPEN_LOOP;
    	}
    }


    // LEDs
    switch(control)
    {
    case 0:
    	PCB_LED3_off();
    	PCB_LED4_off();
    	break;

    case 1:
    	PCB_LED3_off();
    	PCB_LED4_on();
    	break;

    case 2:
    	PCB_LED3_on();
    	PCB_LED4_off();
    	break;

    case 3:
    	PCB_LED3_on();
    	PCB_LED4_on();
    	break;

    default:
    	PCB_LED3_off();
    	PCB_LED4_off();
    }

	// signalize direction change with LED3 and LED4
    if(control_enable_flag == FALSE)
    {
    	if(direction_change_flag == TRUE)
    	{
    		// najprej ugasni obe LED
    		PCB_LED3_off();
    		PCB_LED4_off();

    		delta_tic_direction = interrupt_cnt - tic_direction;
    		if(interrupt_cnt < tic_direction)
    		{
    			delta_tic_direction = delta_tic_direction + SAMPLE_FREQ;
    		}

    		if(direction >= 0)
    		{
    			// za pozitivno smer najprej pri�gi LED4
    			if(delta_tic_direction >= (long)(1*SAMPLE_FREQ/10))
    			{

    				PCB_LED3_off();
    				PCB_LED4_on();
    			}

    			// preklopi LED4
    			if(delta_tic_direction >= (long)(2*SAMPLE_FREQ/10))
    			{
    				PCB_LED4_toggle();
    			}

    			// preklopi LED4
    			if(delta_tic_direction >= (long)(3*SAMPLE_FREQ/10))
    			{
    				PCB_LED4_toggle();
    			}

    			// preklopi LED4
    			if(delta_tic_direction >= (long)(4*SAMPLE_FREQ/10))
    			{
    				PCB_LED4_toggle();
    			}

    			// preklopi LED4
    			if(delta_tic_direction >= (long)(5*SAMPLE_FREQ/10))
    			{
    				PCB_LED4_toggle();
    			}

    			// preklopi obe LED
    			if(delta_tic_direction >= (long)(6*SAMPLE_FREQ/10))
    			{
    				PCB_LED3_off();
    				PCB_LED4_off();
    			}
    		}
    		else
    		{
    			// za negativno smer najprej pri�gi LED3
    			if(delta_tic_direction >= (long)(1*SAMPLE_FREQ/10))
    			{

    				PCB_LED3_on();
    				PCB_LED4_off();
    			}

    			// preklopi LED3
    			if(delta_tic_direction >= (long)(2*SAMPLE_FREQ/10))
    			{
    				PCB_LED3_toggle();
    			}

    			// preklopi LED3
    			if(delta_tic_direction >= (long)(3*SAMPLE_FREQ/10))
    			{
    				PCB_LED3_toggle();
    			}

    			// preklopi LED3
    			if(delta_tic_direction >= (long)(4*SAMPLE_FREQ/10))
    			{
    				PCB_LED3_toggle();
    			}

    			// preklopi LED3
    			if(delta_tic_direction >= (long)(5*SAMPLE_FREQ/10))
    			{
    				PCB_LED3_toggle();
    			}

    			// preklopi obe LED
    			if(delta_tic_direction >= (long)(6*SAMPLE_FREQ/10))
    			{
    				PCB_LED3_off();
    				PCB_LED4_off();
    			}
    		}

    		if(delta_tic_direction >= (long)(9.9*SAMPLE_FREQ/10))
    		{
    			// ne vplivaj ve� na LED
    			direction_change_flag = FALSE;
    			delta_tic_direction = 0;
    		}
    	}
    }
    else
    {
    	// �e je algoritem vodenja �e aktiven, ne rabi� ve� signalizirati smeri vrtenja

		// ne vplivaj ve� na LED
		direction_change_flag = FALSE;
		delta_tic_direction = 0;
    }


    // main conditions for control

    // wait for current offset calibration procedure
    if(current_offset_calibrated_flag == TRUE)
    {
    	// switch 1 means on/off
    	if(sw1_state == FALSE)
    	{
    		SVM_disable();
    		PCB_LED2_off();
    		set_null_position_flag = FALSE;
    		control_enable_flag = FALSE;
    		reset_null_position_procedure_flag = TRUE;
    		incremental_encoder_connected_flag = FALSE;

    		if(hardware_trip_oc_flag == TRUE || software_trip_flag == TRUE)
    		{
    			trip_reset_flag = TRUE;
    		}
    	}
    	else
    	{
    		// switch 1 starts "set null position" procedure
    		if(set_null_position_flag == FALSE)
    		{
    			set_null_position(reset_null_position_procedure_flag);
    			reset_null_position_procedure_flag = FALSE;
    			modulation = SVM;
    		}
    		else
    		{
    			// button 1 enables control alghorithm
    			if(b1_press_int == TRUE && control_enable_flag == FALSE && pot_rel <= 0.5)
    			{
    				control_enable_flag = TRUE;
    				SVM_enable();
    				SVM_update(0.0, 0.0);
    				PCB_LED2_on();
    			}
    			else if(b1_press_int == TRUE && control_enable_flag == TRUE)
    			{
    				SVM_disable();
    				control_enable_flag = FALSE;
    				PCB_LED2_off();
    			} // end of button 1
    		} // end of null position
    	} // end of switch 1
    } // end of current_offset_calibrated


    // if all the conditions are met, control alghorithm becomes active
    if(control_enable_flag == TRUE)
    {
    	control_algorithm();
    }
    else
    {
        // in standby clear all integral parts of controllers
    	id_reg.Ui = 0.0;
    	iq_reg.Ui = 0.0;
    	speed_reg.Ui = 0.0;
    	position_reg.Ui = 0.0;

    	// clear all reference values
    	nap_alpha_ref = 0.0;
    	nap_beta_ref = 0.0;
    	nap_d_ref = 0.0;
    	nap_q_ref = 0.0;
    	tok_d_ref = 0.0;
    	tok_q_ref = 0.0;
    	speed_meh_ref = 0.0;
    	kot_meh_ref = 0.0;

    	// clear all open loop values
    	amp_rel = 0.0;
    	freq_meh = 0.0;
    	duty_six_step = 0.0;
    	duty_DC = 0.0;
    }


    if(trip_reset_flag == TRUE)
    {
    	trip_reset();
    	trip_reset_flag = FALSE;
    }


//    temp1 = cos(2*PI*((float)interrupt_cnt/SAMPLE_FREQ));
//    temp2 = sin(2*PI*((float)interrupt_cnt/SAMPLE_FREQ));




    /* End of 3 phase inverter control alghorithm */




    // store values for display within CCS or GUI
    DLOG_GEN_update();
    
    // check limits
    software_protection();

    /* If overcurrent trip event has occured, shut down power stage
     * and signalise with red LED.
     */
    if(PCB_TRIP_OC_read() == TRUE || SVM_MODUL1.TZSEL.bit.OSHT1)
    {
    	hardware_trip_oc_flag = TRUE;
    }

    if(hardware_trip_oc_flag == TRUE)
    {
    	SVM_trip();
    	PCB_LED1_on();
    	PCB_LED2_off();
    	PCB_LED3_off();
    	PCB_LED4_off();
    }

    /* Test if new interrupt is already waiting.
     * If so, then something is seriously wrong.
     */
    if (EPwm1Regs.ETFLG.bit.INT == TRUE)
    {
        // count number of interrupt overflow events
        interrupt_overflow_counter = interrupt_overflow_counter + 1;

        /* if interrupt overflow event happened more than 10 times
         * stop the CPU
         *
         * Better solution would be to properly handle this event
         * (shut down the power stage, ...)
         */
        if (interrupt_overflow_counter >= 10)
        {
        	SVM_trip();
        	PCB_LED1_on();
        	PCB_LED2_off();
        	PCB_LED3_off();
        	PCB_LED4_off();
            asm(" ESTOP0");
        }
    }
    
    // signalize trip state with red LED
    if(svm_status == TRIP)
    {
    	PCB_LED1_on();
    	PCB_LED2_off();
    	PCB_LED3_off();
    	PCB_LED4_off();
    }
    else
    {
    	PCB_LED1_off();
    }

    // stop the CPU load stopwatch
    TIC_stop();

    // clear buttons, if pressed
    b1_press_int = FALSE;
    b2_press_int = FALSE;
    b3_press_int = FALSE;
    b4_press_int = FALSE;

}   // end of PER_int




/**************************************************************
* Function, where mechanical measurements is handled:
* - mechanical angle [0.0 1.0] (1.0 means one full mechanical revolution)
* - electrical angle [0.0 1.0] (1.0 means one quarter of revolution, if pole pair is 4)
* - calls function for mechanical speed calculation
* - calls function for mechanical acceleration calculation
**************************************************************/
#pragma CODE_SECTION(get_mechanical, "ramfuncs");
void get_mechanical(void)
{
	// lokalne spremenljivke
    int i;


    // signal iz inkrementalnega dajalnika - mehanski kot
    // preberem kot rotorja iz QEP modula
    kot_raw = QEP_cnt();

    // izracunam kot rotorja [1]
    kot_meh = QEP_mehKot();

    // omejim mehanski kot od 0.0 do 1.0
    if (kot_meh < 0.0)
    {
    	kot_meh = kot_meh + 1.0;
    }
    if (kot_meh >= 1.0)
    {
    	kot_meh = kot_meh - 1.0;
    }


    // elektri�ni kot

    // iz mehanskega kota izra�unam �e elektri�nega
    kot_el = POLE_PAIRS*kot_meh;


    // omejim elektri�ni kot od 0.0 do 1.0
    if (kot_el < 0.0)
    {
    	kot_el = kot_el + 1.0;
    }
    // od�tejem zaradi polovih parov P, da dobim kot_el med 0.0 in 1.0
    for (i = POLE_PAIRS - 1; i > 0; i = i - 1)
    {
    	if (kot_el >= i*1.0)
    	{
    		kot_el = kot_el - i*1.0;
    		break;
    	}

    }

    // pokli�em funkcijo, ki vrne mehansko kro�no frekvenco
    get_meh_speed();

    // pokli�em funkcijo, ki vrne mehanski kotni pospe�ek
    // get_meh_accel();

} // end of function



/**************************************************************
* Function, where mechanical speed is calculated (out of mechanical angle)
**************************************************************/
#pragma CODE_SECTION(get_meh_speed, "ramfuncs");
void get_meh_speed(void)
{
	float dusenje_ABF;
	float mejna_frekvenca_ABF;

	// dusenje clena 2. reda [1]
	dusenje_ABF = SQRT2/2.0;
	// mejna frekvenca clena 2. reda [Hz]
	mejna_frekvenca_ABF = 100.0;

	abf_speed_meh.Alpha = ( 2.0*PI*mejna_frekvenca_ABF/ABF_OMEGA_SAMPLING_FREQ )*( 2.0*dusenje_ABF - 2.0*PI*mejna_frekvenca_ABF/(2.0*ABF_OMEGA_SAMPLING_FREQ) );
	abf_speed_meh.Beta = ( 2.0*PI*mejna_frekvenca_ABF/ABF_OMEGA_SAMPLING_FREQ )*( 2.0*PI*mejna_frekvenca_ABF/ABF_OMEGA_SAMPLING_FREQ );
	abf_speed_meh.KotIn = kot_meh;

	ABF_OMEGA_CALC(abf_speed_meh);

	speed_meh = abf_speed_meh.Omega;
}




/**************************************************************
* Function, where mechanical acceleration is calculated (out of mechanical angle)
**************************************************************/
#pragma CODE_SECTION(get_meh_accel, "ramfuncs");
void get_meh_accel(void)
{
/*
	float dusenje_ABF;
	float mejna_frekvenca_ABF;

	// dusenje clena 2. reda [1]
	dusenje_ABF = SQRT2/2.0;
	// mejna frekvenca clena 2. reda [Hz]
	mejna_frekvenca_ABF = 500.0;

	abf_accel_meh.Alpha = ( 2.0*PI*mejna_frekvenca_ABF/ABF_OMEGA_SAMPLING_FREQ )*( 2.0*dusenje_ABF - 2.0*PI*mejna_frekvenca_ABF/(2.0*ABF_OMEGA_SAMPLING_FREQ) );
	abf_accel_meh.Beta = ( 2.0*PI*mejna_frekvenca_ABF/ABF_OMEGA_SAMPLING_FREQ )*( 2.0*PI*mejna_frekvenca_ABF/ABF_OMEGA_SAMPLING_FREQ );
	abf_accel_meh.KotIn = speed_meh;

	ABF_OMEGA_CALC(abf_accel_meh);

	accel_meh_Hz_per_s = abf_accel_meh.Omega;
*/
}




/**************************************************************
* Function, where electrical measurements is handled
**************************************************************/
#pragma CODE_SECTION(get_electrical, "ramfuncs");
void get_electrical(void)
{
    // lokalne spremenljivke

    /* preberem vrednosti iz AD pretvornika */

    // tokovi 1,2,3
    // kalibracija preostalega toka
    if (current_offset_calibrated_flag == FALSE)
    {
        // akumuliram offset
        tok_i1_raw_accu = tok_i1_raw_accu + ADC_CURRENT_1;
        tok_i2_raw_accu = tok_i2_raw_accu + ADC_CURRENT_2;
        tok_i3_raw_accu = tok_i3_raw_accu + ADC_CURRENT_3;

        // ko potece dovolj casa, sporocim da lahko grem naprej
        // in izracunam povprecni offset
        current_offset_counter = current_offset_counter + 1;
        if (current_offset_counter == SAMPLE_FREQ)
        {
            current_offset_calibrated_flag = TRUE;
            tok_i1_raw_offset = tok_i1_raw_accu / SAMPLE_FREQ;
            tok_i2_raw_offset = tok_i2_raw_accu / SAMPLE_FREQ;
            tok_i3_raw_offset = tok_i3_raw_accu / SAMPLE_FREQ;
        }
        tok_i1 = 0.0;
        tok_i2 = 0.0;
        tok_i3 = 0.0;
    }
    else
    {
        tok_i1 = tok_i_gain * (ADC_CURRENT_1 - tok_i1_raw_offset);
        tok_i2 = tok_i_gain * (ADC_CURRENT_2 - tok_i2_raw_offset);
        tok_i3 = tok_i_gain * (ADC_CURRENT_3 - tok_i3_raw_offset);
    }


    // napetosti 1,2,3
    nap_v1 = nap_v_gain * (ADC_VOLTAGE_1 - nap_v1_offset);
    nap_v2 = nap_v_gain * (ADC_VOLTAGE_2 - nap_v2_offset);
    nap_v3 = nap_v_gain * (ADC_VOLTAGE_3 - nap_v3_offset);

    // napetost DC linka
    nap_dc = nap_dc_gain * (ADC_VOLTAGE_DC - nap_dc_offset);


    // pot_rel
    pot_rel = ADC_POT * (1/4096.0);
    // okoli nicle vrzem zeleno vrednost na cisto niclo
    if ((pot_rel > 0.0) && (pot_rel < +0.01))
    {
        pot_rel = 0.0;
    }

    // na potenciometer dodaj se histerezo - manj suma
    // najprej zaokrozim na +-0.01
    pot_rel_discrete = (long)(pot_rel * 100.0);

    // dodaj histerezo
    if (fabs(pot_rel_discrete_old - pot_rel_discrete) < 1)
    {
    	pot_rel_discrete = pot_rel_discrete_old;
    }
    // zgodovina
    // pot_rel_discrete_old = 2*(long)(pot_rel_discrete / 2.0);
    // spravi dol na obmo�je [0,1]
    pot_rel_discrete = pot_rel_discrete / 100.0;
    // zgodovina
    pot_rel_discrete_old = pot_rel_discrete;


    // Clarke-ina transformacija tokov
    clarke_tok.As = tok_i1;
    clarke_tok.Bs = tok_i2;
    CLARKE_FLOAT_CALC(clarke_tok);

    // Park-ova transformacija tokov
    park_tok.Alpha = clarke_tok.Alpha;
    park_tok.Beta = clarke_tok.Beta;
    park_tok.Angle = kot_el;
    PARK_FLOAT_CALC(park_tok);
    tok_d = park_tok.Ds;
    tok_q = park_tok.Qs;

/*
    // izra�un dejanskega navora
    navor_elektromagnetni = 3.0/2.0*POLE_PAIRS*PSI_ROT*tok_q;
    navor_reluktancni = 3.0/2.0*POLE_PAIRS*(Ld - Lq)*tok_d*tok_q;

    navor = navor_elektromagnetni + navor_reluktancni;

    // izra�un dinami�nega navora
    navor_dinamicni = 2*PI* J * alfa_meh;
*/
} // end of function




/**************************************************************
* Function, which alignes d axis with phase 1 axis
**************************************************************/
#pragma CODE_SECTION(set_null_position, "ramfuncs");
void set_null_position(bool reset_procedure)
{
	/* Vnaprej vem, da moram v kratkem stiku na posamezno
	 * fazo PMSM-ja pritisniti 2V, da se bo zavrtel v pravo lego.
	 */
    float 	duty_cycle = 0.2 * 12.0/nap_dc; // z napetostjo DC linka se spreminja
    int 	sector;

    static int	i = 0;
    static long interrupts_passed = 0;

    static unsigned int kot_raw_temp = 0;
    static unsigned int kot_raw_temp_old = 0;

	SVM_enable();
	PCB_LED2_on();

	if(reset_procedure == TRUE)
	{
		i = 0;
		interrupts_passed = 0;
		kot_raw_temp = 0;
	}

    if (i == 0)    // pol sekunde pritiskam napetostni vektor v smeri faze 3
    {
        sector = 5;
        SVM_update_six(duty_cycle, sector);
        interrupts_passed = interrupts_passed + 1;

        if(interrupts_passed > SAMPLE_FREQ/2)
        {
        	i = 1;
        	interrupts_passed = 0;

        	// simple check if incremental encoder is connected
        	kot_raw_temp = QEP_cnt();
        	if(kot_raw_temp - kot_raw_temp_old != 0)
        	{
        		kot_raw_temp_old = kot_raw_temp;
        	}
        }
    } // end of i == 0

    if (i == 1)    // pol sekunde pritiskam napetostni vektor v smeri faze 1
    {
        sector = 1;
        SVM_update_six(duty_cycle, sector);
        interrupts_passed = interrupts_passed + 1;

        if(interrupts_passed > SAMPLE_FREQ/2)
        {
        	i = 2;
        	interrupts_passed = 0;

        	// simple check if incremental encoder is connected
        	kot_raw_temp = QEP_cnt();
        	if(kot_raw_temp - kot_raw_temp_old != 0)
        	{
        		incremental_encoder_connected_flag = TRUE;
        	}
        }
    } // end of i == 1

    if (i == 2)     // �ez nekaj �asa re�em to je pozicija 0, in postavim zastavico
    {
    	interrupts_passed = interrupts_passed + 1;

    	if(interrupts_passed >= SAMPLE_FREQ/2)
    	{
    		set_null_position_flag = TRUE;
    		interrupts_passed = 0;
    		i = 0;

    		// resetiram pozicijo rotorja
    		QEP_reset();

    		// onemogo�im mosti�
    		SVM_disable();
    		PCB_LED2_off();
    	}
    } // end of i == 2

} // end of function




/**************************************************************
* Function, which checks, if voltages and currents are respecting limits
**************************************************************/
#pragma CODE_SECTION(software_protection, "ramfuncs");
void software_protection(void)
{
	// DC link voltage protection
	if(nap_dc > nap_dc_max)
	{
		nap_dc_overvoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}
	else if((nap_dc < nap_dc_min))
	{
		nap_dc_undervoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}

	// phase voltage protection
	if(nap_v1 > nap_v_max)
	{
		nap_v1_overvoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}
	else if((nap_v1 < nap_v_min))
	{
		nap_v1_undervoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}
	if(nap_v2 > nap_v_max)
	{
		nap_v2_overvoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}
	else if((nap_v2 < nap_v_min))
	{
		nap_v2_undervoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}
	if(nap_v3 > nap_v_max)
	{
		nap_v3_overvoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}
	else if((nap_v3 < nap_v_min))
	{
		nap_v3_undervoltage_flag = TRUE;
		software_trip_flag = TRUE;
	}

	// phase curent protection
	if(fabs(tok_i1) > tok_i_max)
	{
		tok_i1_overcurrent_flag = TRUE;
		software_trip_flag = TRUE;
	}
	if(fabs(tok_i2) > tok_i_max)
	{
		tok_i2_overcurrent_flag = TRUE;
		software_trip_flag = TRUE;
	}
	if(fabs(tok_i3) > tok_i_max)
	{
		tok_i3_overcurrent_flag = TRUE;
		software_trip_flag = TRUE;
	}

	// trip execution
	if(software_trip_flag == TRUE)
	{
		SVM_trip();
		PCB_LED1_on();
		PCB_LED2_off();
		PCB_LED3_off();
		PCB_LED4_off();
	}

} // end of function




/**************************************************************
* Function, which covers control of 3 phase PMSM
**************************************************************/
#pragma CODE_SECTION(control_algorithm, "ramfuncs");
void control_algorithm(void)
{
	switch(control)
	{
	case OPEN_LOOP:
		open_loop_control();
		break;
	case CURRENT_CONTROL:
		current_loop_control();
		break;
	case SPEED_CONTROL:
		speed_loop_control();
		break;
	case POSITION_CONTROL:
		position_loop_control();
		break;
	default:
		SVM_disable();
		control_enable_flag = FALSE;
		PCB_LED2_off();
		break;
	}

	switch(modulation)
	{
	case SVM:
		SVM_update(nap_alpha_ref, nap_beta_ref);
		break;
	case SIX_STEP:
		SVM_update_six(duty_six_step, sector_six_step);
		break;
	case SINGLE_PHASE_DC:
		SVM_update_DC(duty_DC);
		break;
	default:
		SVM_disable();
		control_enable_flag = FALSE;
		PCB_LED2_off();
		break;
	}
} // end of function




/**************************************************************
* Function for open loop control
**************************************************************/
#pragma CODE_SECTION(open_loop_control, "ramfuncs");
void open_loop_control(void)
{
	if(modulation == SVM)
	{
		if(incremental_encoder_connected_flag == FALSE)
		{
			// if incremental encoder is NOT connnected

			// potenciometer is changing duty cycle
			amp_rel = pot_rel;

			// button 3 and 4 are changing mechanical freqency
			if(b3_press_int == TRUE)
			{
				freq_meh = freq_meh - 0.5;
			}
			else if(b4_press_int == TRUE)
			{
				freq_meh = freq_meh + 0.5;
			}

			// omejitev na [0,speed_ref_max]
			if(freq_meh > speed_ref_max)
			{
				freq_meh = speed_ref_max;
			}
			else if(freq_meh < 0.0)
			{
				freq_meh = 0.0;
			}

			freq = POLE_PAIRS * freq_meh;

			nap_alpha_ref = amp_rel*cos(2*PI*freq*interrupt_cnt/SAMPLE_FREQ);
			nap_beta_ref =  direction*amp_rel*sin(2*PI*freq*interrupt_cnt/SAMPLE_FREQ);
		}
		else
		{
			// if incremental encoder is connnected
			amp_rel = direction*pot_rel*0.577;

			ipark_nap.Ds = 0.0;
			ipark_nap.Qs = amp_rel;
			ipark_nap.Angle = kot_el;

			IPARK_FLOAT_CALC(ipark_nap);

			nap_alpha_ref = ipark_nap.Alpha;
			nap_beta_ref = ipark_nap.Beta;
		}
	}
	else if(modulation == SIX_STEP)
	{
		// button 3 and 4 are changing duty cycle
		if(b3_press_int == TRUE)
		{
			duty_six_step = duty_six_step - 0.01;
		}
		else if(b4_press_int == TRUE)
		{
			duty_six_step = duty_six_step + 0.01;
		}

		// omejitev na [0,1]
		if(duty_six_step > 1.0)
		{
			duty_six_step = 1.0;
		}
		else if(duty_six_step < 0.0)
		{
			duty_six_step = 0.0;
		}

		// dolo�anje smeri s potenciometrom
		if(direction >= 0)
		{
			if(pot_rel > 0.0)
			{
				sector_six_step = 1;
			}
			if(pot_rel > 0.15)
			{
				sector_six_step = 2;
			}
			if(pot_rel > 0.30)
			{
				sector_six_step = 3;
			}
			if(pot_rel > 0.45)
			{
				sector_six_step = 4;
			}
			if(pot_rel > 0.60)
			{
				sector_six_step = 5;
			}
			if(pot_rel > 0.75)
			{
				sector_six_step = 6;
			}
		}
	}
	else if(modulation == SINGLE_PHASE_DC)
	{
		duty_DC = direction*pot_rel;
	}
}




/**************************************************************
* Function for current loop control
**************************************************************/
#pragma CODE_SECTION(current_loop_control, "ramfuncs");
void current_loop_control(void)
{
	if(modulation == SVM && incremental_encoder_connected_flag == TRUE)
	{
		// omejim �elene tokove za vsak slu�aj, �e �e prej nisem


		// pazim na I del regulatorja, ko med delovanjem izklopim mosti�
		if (svm_status != ENABLE)
		{
			// �e je mosti� onemogo�en, po�istim �e integralne dele regulatorjev
			id_reg.Ui = 0.0;
			iq_reg.Ui = 0.0;
		}

		// samo, �e je izbran re�im tokovne regulacije, definiraj referenco q toka
		if(control == CURRENT_CONTROL)
		{
			tok_d_ref = 0.0;
			tok_q_ref = direction*pot_rel*tok_q_ref_max;
		}

		// tokovna PI regulacija - d os
		id_reg.Ref = tok_d_ref;
		id_reg.Fdb = tok_d;
		id_reg.Kp = Kp_id_reg * 12.0/nap_dc; // z napetostjo DC linka se spreminja
		id_reg.Ki = Ki_id_reg * 12.0/nap_dc; // z napetostjo DC linka se spreminja
		id_reg.OutMax = nap_d_ref_max;
		id_reg.OutMin = nap_d_ref_min;
		PI_ctrl_calc(&id_reg);

		nap_d_ref = id_reg.Out;


		// tokovna PI regulacija - q os
		iq_reg.Ref = tok_q_ref;
		iq_reg.Fdb = tok_q;
		iq_reg.Kp = Kp_iq_reg * 12.0/nap_dc; // z napetostjo DC linka se spreminja
		iq_reg.Ki = Ki_iq_reg * 12.0/nap_dc; // z napetostjo DC linka se spreminja
		iq_reg.OutMax = nap_q_ref_max;
		iq_reg.OutMin = nap_q_ref_min;
		PI_ctrl_calc(&iq_reg);

		nap_q_ref = iq_reg.Out;

		// izracun napetosti za SVM z inverzno Parkovo transformacijo
		ipark_nap.Ds = nap_d_ref;
		ipark_nap.Qs = nap_q_ref;
		ipark_nap.Angle = kot_el;

		IPARK_FLOAT_CALC(ipark_nap);

		nap_alpha_ref = ipark_nap.Alpha;
		nap_beta_ref = ipark_nap.Beta;
	} // end of if(modulation == SVM)
	else
	{
		SVM_disable();
		control_enable_flag = FALSE;
		PCB_LED2_off();
	}

} // end of function




/**************************************************************
* Function for speed loop control
**************************************************************/
#pragma CODE_SECTION(speed_loop_control, "ramfuncs");
void speed_loop_control(void)
{
	if(modulation == SVM && incremental_encoder_connected_flag == TRUE)
	{
		// omejim �eleno hitrost za vsak slu�aj, �e �e prej nisem


		// pazim na I del regulatorja, ko med delovanjem izklopim mosti�
		if (svm_status != ENABLE)
		{
			// �e je mosti� onemogo�en, po�istim �e integralne dele regulatorjev
			speed_reg.Ui = 0.0;
		}

		// samo, �e je izbran re�im hitrostne regulacije, definiraj referenco hitrosti
		if (control == SPEED_CONTROL)
		{
			speed_meh_ref = direction*pot_rel*speed_ref_max;
		}
			// hitrostna PI regulacija
			speed_reg.Ref = speed_meh_ref;
			speed_reg.Fdb = speed_meh;
			speed_reg.Kp = Kp_speed_reg;
			speed_reg.Ki = Ki_speed_reg;
			speed_reg.OutMax = tok_q_ref_max;
			speed_reg.OutMin = tok_q_ref_min;
			PI_ctrl_calc(&speed_reg);

			tok_q_ref = speed_reg.Out;

			// tokovna PI regulacija
			current_loop_control();

	} // end of modulation: SVM
	else
	{
		SVM_disable();
		PCB_LED2_off();
	}

} // end of speed_loop_control




/**************************************************************
* Function for position loop control
**************************************************************/
#pragma CODE_SECTION(position_loop_control, "ramfuncs");
void position_loop_control(void)
{
	if(modulation == SVM && incremental_encoder_connected_flag == TRUE)
	{
		// omejim �elene tokove za vsak slu�aj, �e �e prej nisem


		// pazim na I del regulatorja, ko med delovanjem izklopim mosti�
		if (svm_status != ENABLE)
		{
			// �e je mosti� onemogo�en, po�istim �e integralne dele regulatorjev
			position_reg.Ui = 0.0;
		}

		// samo, �e je izbran re�im pozicijske regulacije, definiraj referenco kota
		if (control == POSITION_CONTROL)
		{
			kot_meh_ref = pot_rel_discrete;
		}

		// pozicijska PID regulacija
		position_reg.Fdc = 1000.0;							// differential filter cuttof frequency
		position_reg.Kff = 0.0;								// Parameter: Feedforward gain
		position_reg.Sampling_period = 1.0/SAMPLE_FREQ;     // sampling period

		// korigiraj mejni primer, ko gre kot nad 1.0 in pod 0.0
		if(kot_meh_ref - abf_speed_meh.KotOut > 0.5)
		{
			kot_meh_ref = kot_meh_ref - 1.0;
		}
		else if(kot_meh_ref - abf_speed_meh.KotOut < -0.5)
		{
			kot_meh_ref = kot_meh_ref + 1.0;
		}

		position_reg.Ref = kot_meh_ref;
		position_reg.Fdb = abf_speed_meh.KotOut;
		position_reg.Kp = Kp_position_reg;
		position_reg.Ki = Ki_position_reg;
		position_reg.Kd = Kd_position_reg;
		position_reg.OutMax = tok_q_ref_max;
		position_reg.OutMin = tok_q_ref_min;
		PID_CTRL_CALC(position_reg);

 		tok_q_ref = position_reg.Out;

		current_loop_control();

	} // end of modulation: SVM
	else
	{
		SVM_disable();
		PCB_LED2_off();
	}

} // end of position_loop_control




/**************************************************************
* Function, which resets control alghorithm after trip
**************************************************************/
void trip_reset(void)
{
	// clear hardware trip flags
	TRIP_OC_reset();
	// disable PWM
	SVM_disable();
	
	// clear all flags except 2
	current_offset_calibrated_flag = TRUE;
	reset_null_position_procedure_flag = TRUE;
	
	control_enable_flag = FALSE;
	
	set_null_position_flag = FALSE;
	incremental_encoder_connected_flag = FALSE;
	direction_change_flag = FALSE;
	
	hardware_trip_oc_flag = FALSE;
	software_trip_flag = FALSE;
	
	nap_dc_overvoltage_flag = FALSE;
	nap_dc_undervoltage_flag = FALSE;
	nap_v1_overvoltage_flag = FALSE;
	nap_v1_undervoltage_flag = FALSE;
	nap_v2_overvoltage_flag = FALSE;
	nap_v2_undervoltage_flag = FALSE;
	nap_v3_overvoltage_flag = FALSE;
	nap_v3_undervoltage_flag = FALSE;
	tok_i1_overcurrent_flag = FALSE;
	tok_i2_overcurrent_flag = FALSE;
	tok_i3_overcurrent_flag = FALSE;

	// shut off all LEDs
	PCB_LED1_off();
	PCB_LED2_off();
	PCB_LED3_off();
	PCB_LED4_off();
	
	// clear all integral parts of controllers
	id_reg.Ui = 0.0;
	iq_reg.Ui = 0.0;
	speed_reg.Ui = 0.0;
	position_reg.Ui = 0.0;

	// clear all reference values
	nap_alpha_ref = 0.0;
	nap_beta_ref = 0.0;
	nap_d_ref = 0.0;
	nap_q_ref = 0.0;
	tok_d_ref = 0.0;
	tok_q_ref = 0.0;
	speed_meh_ref = 0.0;
	kot_meh_ref = 0.0;

	// clear all open loop values
	amp_rel = 0.0;
	freq_meh = 0.0;
	duty_six_step = 0.0;
	sector_six_step = 1;
	duty_DC = 0.0;
	
	// set variables to initial state
	direction = 1;
	tic_direction = 0;
	delta_tic_direction = 0;
	
	modulation = SVM;
	control = OPEN_LOOP;
	
	kot_raw = 0;
	kot_meh = 0.0;
	
	
}




/**************************************************************
* Function which initializes all required for execution of
* interrupt function
**************************************************************/
void PER_int_setup(void)
{
    // initialize data logger
    dlog.mode = Normal;
    dlog.auto_time = 1;
    dlog.holdoff_time = 1;

    dlog.downsample_ratio = 10;

    dlog.slope = Positive;
    dlog.trig = &kot_meh;
    dlog.trig_level = 0.01;

    dlog.iptr1 = &tok_d;
    dlog.iptr2 = &tok_q;
    dlog.iptr3 = &speed_meh;
    dlog.iptr4 = &kot_meh;

    // initialize reference generator
    ref_gen.type = REF_Step;
    ref_gen.amp = 1.0;
    ref_gen.offset = 0.0;
    ref_gen.freq = 1.0;
    ref_gen.duty = 0.5;
    ref_gen.slew = 100;
    ref_gen.samp_period = SAMPLE_TIME;
    
    // initialize stopwatch
    TIC_init();

    // setup interrupt trigger
    EPwm1Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;
    EPwm1Regs.ETPS.bit.INTPRD = ET_1ST;
    EPwm1Regs.ETCLR.bit.INT = 1;
    EPwm1Regs.ETSEL.bit.INTEN = 1;

    // register the interrupt function
    EALLOW;
    PieVectTable.EPWM1_INT = &PER_int;
    EDIS;

    // acknowledge any spurious interrupts
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;

    // enable interrupt within PIE
    PieCtrlRegs.PIEIER3.bit.INTx1 = 1;

    // enable interrupt within CPU
    IER |= M_INT3;

    // enable interrupt in real time mode
    SetDBGIER(M_INT3);
}

