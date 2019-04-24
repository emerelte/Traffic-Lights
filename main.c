#include <avr/io.h>
#include <avr/interrupt.h>

//ustawienia
const unsigned char zielone=10;			// jak długo domyślnie potrwa zielone
volatile unsigned char temp_zielone;
const unsigned char m_del=98;			// światło będzie mrugać co 0.25 s (wzór: 40*1024*m_del/16000000)
const unsigned char repeat=5;			// jak dużo mrugnięć zielonym światłem przed zakończeniem
const unsigned char tolerancja=1;		// ile sekund tolerancji na czerwonym
volatile unsigned char temp_tolerancja;
volatile unsigned int etap=0;			// od którego etapu zacząć (etap+1)

//do 7 seg oraz pętli czekających
const int tab[] = {0x84, 0xF5, 0x4C, 0x64, 0x35, 0x26, 0x06, 0xF4, 0x04, 0x24, 0xFF};//do 7seg: tab[0-9] cyfry tab[10] nic nie wyświetlaj
volatile char temp_liczba;				// którą liczbę wyświetlać
volatile unsigned int i;				// do pętli czekającej
volatile unsigned int j;

//funkcja opóźniająca o 40*1024*N/clk_freq sekund
void wait(unsigned char N)
{
	for(i = 0; i<40; i++){
		TCNT0=0xFF-N;					//ładuje początkową wartość timera
		TCCR0B=(1<<CS02)|(1<<CS00);		//Timer start. Ustawia clk/1024
		while((TIFR0 & (1<<TOV0))==0 ); //oczekuje na timer
		TCCR0B=0;						//Timer stop
		TIFR0=(1<<TOV0);				//Ustawia flagę overflow na 1
	}
}

//wyświetla podaną cyfrę (x), na podanym wyświetlaczu (0-3)
void wyswietl_cyfre(char x, char pos){
	switch(pos){
		case 0:
		PORTC=0b001110;
		break;
		case 1:
		PORTC=0b001101;
		break;
		case 2:
		PORTC=0b001011;
		break;
		case 3:
		PORTC=0b000111;
		break;
	}
	PORTD=tab[(int)x];
}

//wyswietla podaną liczbę (x) przez ok. 1 s
void wyswietl_liczbe(unsigned char x){
	int ilosc_cyfr=0;unsigned char temp_x=x;
		do{
			temp_x/=10;
			ilosc_cyfr++;
		}while(temp_x!=0);	
	char cyfry[ilosc_cyfr];
	temp_x=x;
	for(int a=0;a<ilosc_cyfr;a++){
		cyfry[a]=temp_x%10;
		temp_x-=temp_x%10;temp_x/=10;
	}
	for(j=0;j<200/ilosc_cyfr;j++){
		for(char a=0;a<ilosc_cyfr;a++){
				wyswietl_cyfre(cyfry[(int)a],a);
				wait(1);
			}
	}
}

//wyświetla zielone dla podanego kierunku
void green(unsigned char version){
	if (version==0)
		PORTB=((1<<0)|(1<<3));
	else if (version==1)
		PORTB=((1<<1)|(1<<2));
}

//miga zielonym podanego kierunku
void toggle(unsigned char version){
	for(char a=0;a<4;a++)
		wyswietl_cyfre(10,a);
	unsigned char bit_nr;
	if (version==0)
		bit_nr=0;
	else if (version==1)
		bit_nr=2;
	else
		bit_nr=0;
	green(version);
	for(int i = 0; i<2*repeat;i++){
		PORTB^=(1<<bit_nr);
		wait(m_del);
	}
}

//oba czerwone
void bothRed(void){
	PORTB=(1<<1)|(1<<3);
}

//na podstawie etapu, decyduje co robić
void dalej(unsigned int etap_m){
	switch (etap_m){
		case 0:
		case 3:
			green(etap_m%2);
			break;
		case 1:
		case 4:
			toggle((etap_m+1)%2);
			break;
		case 2:
		case 5:
			bothRed();
			break;
	}
}

// co ok. 1 sekundę odzywa się przerwanie od timer1
ISR(TIMER1_OVF_vect)
{
	TCNT1=0x0BDC;
	if((etap%3==2) && (tolerancja==0))
		etap=(etap+1)%6;
	if ((etap%3==0) && (zielone==0))
		etap=(etap+1)%6;
	dalej(etap);
	if ((etap%3==0) && temp_zielone>0){
		wyswietl_liczbe(temp_liczba);
		temp_liczba--;
		temp_zielone--;
		temp_tolerancja=tolerancja-1;
	}
	else if ((etap%3==2) && temp_tolerancja>0){
		for(char a=0;a<4;a++)
			wyswietl_cyfre(10,a);
		temp_tolerancja--;	
		temp_zielone=zielone-1;
		temp_liczba=zielone;
	}
	else if (etap%3==0 && temp_zielone==0){
		wyswietl_liczbe(temp_liczba);	
		temp_zielone=zielone-1;
		temp_liczba=zielone;
		temp_tolerancja=tolerancja-1;
		etap=(etap+1)%6;
	}
	else{
		temp_tolerancja=tolerancja-1;
		temp_zielone=zielone-1;
		etap=(etap+1)%6;
	}
}

ISR(INT0_vect){
	for(int a=0;a<4;a++)
		wyswietl_cyfre(10,a);
	temp_liczba=zielone;
	if(etap%3==0)
		etap=(etap+1)%6;
}

int main(void)
{
	temp_zielone=zielone-1;
	temp_tolerancja=tolerancja-1;
	temp_liczba=zielone;
    DDRB=0xFF;		//PORTB jako wyjście dla świateł
	DDRD=0xFB;		//PORTD jako wyjście dla katod
	DDRC=0xFF;		//PORTC jako wyjście dla anod
	PORTD=0xFF;
	// TIMER1 ujawnia się co sekundę
	cli();         // disable global interrupts
	TCCR1A = 0;    // set entire TCCR1A register to 0
	TCCR1B = 0;    // set entire TCCR1B register to 0
	TCNT1=0x0BDC;  // żeby było co sekundę
	TIMSK1 |= (1 << TOIE1); //enable timer1 interrupt
	TCCR1B |= (1 << CS12); // Sets bit CS12 in TCCR1B (/256 prescaling)
	
	//zmiana kierunku świateł
	EICRA|=(1<<ISC01);
	EIMSK|=(1<<INT0);
	sei();
	
	while (1) 
    {
    }
}
