#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#define A PB0
#define B PB1
#define C PB2
#define D PB3
#define E PB4
#define F PB5
#define G PB6
#define DP PB7

#define RTC_ADDR (0x68 << 1)

void init_display(){
    DDRC = 0b1111; // PC0-3 outputs
    PORTC = 0;

    // PB all inputs until they sink
    DDRB = 0;
    PORTB = 0;

    // Setup timer to automatically update display
    TCCR0 = (1 << CS00);
    TIMSK |= (1 << TOIE0);
}

void draw_number(char num){
    int result = 0;

    switch(num){
        case 0:
            result = ~(1 << G);
            break;

        case 1:
            result = (1 << B) | (1 << C);
            break;

        case 2:
            result = ~((1 << F) | (1 << C));
            break;

        case 3:
            result = ~((1 << F) | (1 << E));
            break;

        case 4:
            result = ~((1 << A) | (1 << E) | (1 << D));
            break;

        case 5:
            result = ~((1 << B) | (1 << E));
            break;
    
        case 6:
            result = ~(1 << B);
            break;

        case 7:
            result = (1 << A) | (1 << B) | (1 << C);
            break;

        case 8:
            result = 0xff;
            break;

        case 9:
            result = ~(1 << E); 
            break;

        case 'a':
            result = ~(1 << D);
            break;

        case 'b':
            result = ~((1 << A) | (1 << B));
            break;

        case 'c':
            result = (1 << G) | (1 << E) | (1 << D);
            break;

        case 'd':
            result = ~((1 << A) | (1 << F));
            break;

        case 'e':
            result = ~((1 << B) | (1 << C));
            break;

        case 'f':
            result = ~((1 << B) | (1 << C) | (1 << D));
            break;
    }

    result &= ~(1 << DP);
    DDRB = result;
}

void clear_display(){
    DDRB = 0;
}

unsigned char display[] = {'-', 6, 2, 3};
int digit = 0;
void update_display(){
    // buffer display in case of changes
    char buffer[4];
    for(int i = 0; i < 4; i++){
        buffer[i] = display[i];
    }

    clear_display();

    // Select digit
    PORTC = (1 << digit);
    draw_number(display[digit]);

    // Decimal point
    if(1 == digit)
        DDRB |= (1 << DP);

    digit++;

    if(digit > 3){
        digit = 0;
    }
}

ISR (TIMER0_OVF_vect) {
    update_display();
}

void init_TWI(){
    TWBR = 10; // SCL 10kHz
}

void stop_TWI(){
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    display[1] = 1;
}

void start_TWI(int addr){
    TWCR |= (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

    // Start
    while (!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8) != TW_START){
        return;
    }

    // Slave write Address
    TWDR = addr;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while(!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8) != TW_MT_SLA_ACK && (TWSR & 0xF8) != TW_MR_SLA_ACK){
        return;
    }
}

void write_RTC(int addr, int data) {
    start_TWI(RTC_ADDR);

    // Write first address
    TWDR = addr;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8) != TW_MT_DATA_ACK)
        return;

    // Write first data
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8) != TW_MT_DATA_ACK)
        return;

    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}

char read_RTC(int addr) {
    start_TWI(RTC_ADDR);

    // Write first address
    TWDR = addr;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8) != TW_MT_DATA_ACK){
        return 0;
    }

    // Repeated start
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

    // Wait for start
    while(!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8 != TW_START)){
        return 0;
    }

    // Send slave Address
    TWDR = RTC_ADDR | 1;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while(!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8) != TW_MR_SLA_ACK){
        return 0;
    }

    TWCR = (1 << TWINT) | (1 << TWEN);
    while(!(TWCR & (1 << TWINT)));
    if((TWSR & 0xF8) != TW_MR_DATA_NACK){
        return 0;
    }
    char temp = TWDR;

    stop_TWI();
    return temp;
}

int main(void){
    init_display();
    init_TWI();
    sei();

    DDRD &= ~(1 << PD7);
    PORTD |= (1 << PD7); // Pullup

    // We need to alter CH and 24 hour bits without chaning time
    char mins = read_RTC(0);
    mins &= ~(1 << 7);
    write_RTC(0, mins);

    char hours = read_RTC(2);
    hours &= ~(1 << 6);
    write_RTC(2, hours);

    while(1){
        /* Convert minutes and hours into pure binary instead of BCD */
        // Minutes
        char temp;
        temp = read_RTC(1);
        char m = (temp & 0b1111) + ((temp & 0b1110000) >> 4) * 10;

        // Hours
        temp = read_RTC(2);
        char h = (temp & 0b1111) + ((temp & 0b1110000) >> 4) * 10;

        if (!(PIND & (1 << PD7))){
            m++;
            if(m >= 60){
                m = 0;
                h++;
            }

            if(h > 24){
                h = 0;
            }

            // Save time to RTC (back in BCD)
            write_RTC(1, (m % 10) | (m / 10) << 4);
            write_RTC(2, (h % 10) | (h / 10) << 4);
        }

        // Update display accordingly
        display[1] = h % 10;
        display[0] = h / 10;
        display[3] = m % 10;
        display[2] = m / 10;

        _delay_ms(25);
    }
}
