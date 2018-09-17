#include "miuchiz.h"
// https://www.dropbox.com/s/nmf2b9am4p6ptr6/cpu6502.py?dl=0 used as a guide

#define FLAG_CARRY    1
#define FLAG_ZERO     2
#define FLAG_NO_IRQ   4
#define FLAG_DECIMAL  8
#define FLAG_OVERFLOW 64
#define FLAG_NEGATIVE 128

typedef uint16_t (*address_mode)(struct cpu_state *s);
typedef void (*memory_op)(struct cpu_state *s, uint8_t value);

// ------------------------------------------------------------------

uint8_t get_instruction_byte(struct cpu_state *s) {
  return s->read(s->hardware, s->pc++);
}

uint16_t zeropage(struct cpu_state *s) {
  return get_instruction_byte(s);
}

uint16_t zeropage_x(struct cpu_state *s) {
  return (get_instruction_byte(s) + s->x) & 0xff;
}

uint16_t zeropage_y(struct cpu_state *s) {
  return (get_instruction_byte(s) + s->y) & 0xff;
}

uint16_t absolute(struct cpu_state *s) {
  uint8_t low = get_instruction_byte(s);
  uint8_t high = get_instruction_byte(s);
  return (high << 8) | low;
}

uint16_t absolute_x(struct cpu_state *s) {
  uint8_t low = get_instruction_byte(s);
  uint8_t high = get_instruction_byte(s);
  return ((high << 8) | low) + s->x;
}

uint16_t absolute_y(struct cpu_state *s) {
  uint8_t low = get_instruction_byte(s);
  uint8_t high = get_instruction_byte(s);
  return ((high << 8) | low) + s->y;
}

uint16_t indirect(struct cpu_state *s) {
  uint8_t zp = get_instruction_byte(s);
  uint8_t low = s->read(s->hardware, zp);
  uint8_t high = s->read(s->hardware, zp+1);
  return ((high << 8) | low);
}

uint16_t indirect_x(struct cpu_state *s) {
  uint8_t zp = get_instruction_byte(s);
  uint8_t low = s->read(s->hardware, (zp+s->x)&0xff);
  uint8_t high = s->read(s->hardware, (zp+s->x+1)&0xff);
  return ((high << 8) | low);
}

uint16_t indirect_y(struct cpu_state *s) {
  uint8_t zp = get_instruction_byte(s);
  uint8_t low = s->read(s->hardware, zp);
  uint8_t high = s->read(s->hardware, zp+1);
  return ((high << 8) | low) + s->y;
}

void push(struct cpu_state *s, uint8_t value) {
  s->write(s->hardware, 0x100+(s->s--), value);
}

uint8_t pop(struct cpu_state *s) {
  return s->read(s->hardware, 0x100+(++s->s));
}

int sign_extend(uint8_t t) {
  if(t & 0x80)
    return t | ~0xff;
  return t;
}

void branch(struct cpu_state *s, uint8_t amount) {
  s->pc += sign_extend(amount);
}

// ------------------------------------------------------------------

void update_z(struct cpu_state *s, uint8_t value) {
  s->flags &= ~(FLAG_ZERO);
  if(value == 0)
    s->flags |= FLAG_ZERO;
}

void update_nz(struct cpu_state *s, uint8_t value) {
  s->flags &= ~(FLAG_ZERO | FLAG_NEGATIVE);
  if(value == 0)
    s->flags |= FLAG_ZERO;
  if(value & 128)
    s->flags |= FLAG_NEGATIVE;
}

void op_ora(struct cpu_state *s, uint8_t value) {
  s->a |= value;
  update_nz(s, s->a);
}

void op_and(struct cpu_state *s, uint8_t value) {
  s->a &= value;
  update_nz(s, s->a);
}

void op_eor(struct cpu_state *s, uint8_t value) {
  s->a ^= value;
  update_nz(s, s->a);
}

void op_adc(struct cpu_state *s, uint8_t value) {
  int carry = (s->flags & FLAG_CARRY)?1:0;
  if(s->flags & FLAG_DECIMAL) {
    int lowresult = (s->a & 0x0F) + (value & 0x0F) + carry;
    if(lowresult > 9)
      lowresult = ((lowresult + 6) & 0xF)+16;
    int result = (s->a & 0xF0) + (value & 0xF0) + lowresult;
    if(result >= 0xa0)
      result += 0x60;

     s->flags &= ~(FLAG_CARRY | FLAG_OVERFLOW);
     if(result > 255)
       s->flags |= FLAG_CARRY;
     int twos = sign_extend(value & 0xf0) + sign_extend(s->a & 0xf0) + lowresult;
    
     value = result & 0xff;

     if(twos < -128 || twos > 127)
       s->flags |= FLAG_OVERFLOW;
  } else {
     int twos = sign_extend(value) + sign_extend(s->a) + carry;
     int result = value = s->a + carry;
     s->flags &= ~(FLAG_CARRY | FLAG_OVERFLOW);
     if(result > 255)
       s->flags |= FLAG_CARRY;

     value = result & 0xff;

     if(twos < -128 || twos > 127)
       s->flags |= FLAG_OVERFLOW;
  }

  s->a = value;
  update_nz(s, s->a);
}

void op_lda(struct cpu_state *s, uint8_t value) {
  s->a = value;
  update_nz(s, s->a);
}

void op_ldx(struct cpu_state *s, uint8_t value) {
  s->x = value;
  update_nz(s, s->x);
}

void op_ldy(struct cpu_state *s, uint8_t value) {
  s->y = value;
  update_nz(s, s->y);
}

void op_bit(struct cpu_state *s, uint8_t value) {
  s->flags &= ~(FLAG_ZERO | FLAG_NEGATIVE | FLAG_OVERFLOW);
  if(value & 128)
    s->flags |= FLAG_NEGATIVE;
  if(value & 64)
    s->flags |= FLAG_OVERFLOW;
  if((s->a & value) == 0)
    s->flags |= FLAG_ZERO;
}


void compare(struct cpu_state *s, uint8_t reg, uint8_t value) {
  s->flags &= ~(FLAG_ZERO | FLAG_NEGATIVE | FLAG_CARRY);
  if(reg == value) 
    s->flags |= FLAG_ZERO;
  if(reg >= value) 
    s->flags |= FLAG_CARRY;
  if((reg - value) & 0x80) 
    s->flags |= FLAG_NEGATIVE;
}

void op_cmp(struct cpu_state *s, uint8_t value) {
  compare(s, s->a, value);
}

void op_cpx(struct cpu_state *s, uint8_t value) {
  compare(s, s->x, value);
}

void op_cpy(struct cpu_state *s, uint8_t value) {
  compare(s, s->y, value);
}

void op_sbc(struct cpu_state *s, uint8_t value) {
  // maybe decimal mode is different?
  op_adc(s, value ^ 255);
}


// ------------------------------------------------------------------

void run_instruction(struct cpu_state *s) {
  if(s->waiting)
    return;
  uint8_t opcode = get_instruction_byte(s);
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%.2x PC:%.4x A:%.2x X:%.2x Y:%.2x", opcode, s->pc, s->a, s->x, s->y);

  // decode the instruction
  int aaa = opcode >> 5;
  int bbb = (opcode >> 2) & 7;
  int cc = opcode & 3;
  uint8_t value = 0;
  uint16_t address = 0;
  int temp;

  static const memory_op main_ops[] = {
     op_ora, op_and, op_eor, op_adc, NULL, op_lda, op_cmp, op_sbc
  };

  // try instructions that don't fit the regular patterns first
  switch(opcode) {

    case 0x04: // tsb zp
      address = zeropage(s);
      temp = s->read(s->hardware, address);
      update_z(s, s->a & temp);
      s->write(s->hardware, address, temp | s->a);
      return;
    case 0x14: // trb zp
      address = zeropage(s);
      temp = s->read(s->hardware, address);
      update_z(s, s->a & temp);
      s->write(s->hardware, address, temp & ~s->a);
      return;
    case 0x0c: // tsb abs
      address = absolute(s);
      temp = s->read(s->hardware, address);
      update_z(s, s->a & temp);
      s->write(s->hardware, address, temp | s->a);
      return;
    case 0x1c: // trb abs
      address = absolute(s);
      temp = s->read(s->hardware, address);
      update_z(s, s->a & temp);
      s->write(s->hardware, address, temp & ~s->a);
      return;

    case 0x64: // stz zp
      s->write(s->hardware, zeropage(s), 0);
      return;
    case 0x74: // stz zp,x
      s->write(s->hardware, zeropage_x(s), 0);
      return;
    case 0x9c: // stz abs
      s->write(s->hardware, absolute(s), 0);
      return;
    case 0x9e: // stz abs,x
      s->write(s->hardware, absolute_x(s), 0);
      return;


    case 0x84: // sty zp
      s->write(s->hardware, zeropage(s), s->y);
      return;
    case 0x94: // sty zp,x
      s->write(s->hardware, zeropage_x(s), s->y);
      return;
    case 0x8c: // sty abs
      s->write(s->hardware, absolute(s), s->y);
      return;

    case 0xac: // ldy abs
      op_ldy(s, s->read(s->hardware, absolute(s)));
      return;
    case 0xbc: // ldy abs,x
      op_ldy(s, s->read(s->hardware, absolute_x(s)));
      return;


    case 0xa4: // ldy zp
      op_ldy(s, s->read(s->hardware, zeropage(s)));
      return;
    case 0xb4: // ldy zp,x
      op_ldy(s, s->read(s->hardware, zeropage_x(s)));
      return;
    case 0xc4: // cpy zp
      op_cpy(s, s->read(s->hardware, zeropage(s)));
      return;
    case 0xe4: // cpx zp
      op_cpx(s, s->read(s->hardware, zeropage(s)));
      return;

    case 0xcc: // cpy abs
      op_cpy(s, s->read(s->hardware, absolute(s)));
      return;
    case 0xec: // cpx abs
      op_cpx(s, s->read(s->hardware, absolute(s)));
      return;


    case 0x10: // bpl
      temp = get_instruction_byte(s);
      if(!(s->flags & FLAG_NEGATIVE))
        branch(s, temp);
      return;
    case 0x30: // bmi
      temp = get_instruction_byte(s);
      if(s->flags & FLAG_NEGATIVE)
        branch(s, temp);
      return;
    case 0x50: // bvc
      temp = get_instruction_byte(s);
      if(!(s->flags & FLAG_OVERFLOW))
        branch(s, temp);
      return;
    case 0x70: // bvs
      temp = get_instruction_byte(s);
      if(s->flags & FLAG_OVERFLOW)
        branch(s, temp);
      return;
    case 0x90: // bcc
      temp = get_instruction_byte(s);
      if(!(s->flags & FLAG_CARRY))
        branch(s, temp);
      return;
    case 0xB0: // bcs
      temp = get_instruction_byte(s);
      if(s->flags & FLAG_CARRY)
        branch(s, temp);
      return;
    case 0xD0: // bne
      temp = get_instruction_byte(s);
      if(!(s->flags & FLAG_ZERO))
        branch(s, temp);
      return;
    case 0xF0: // beq
      temp = get_instruction_byte(s);
      if(s->flags & FLAG_ZERO)
        branch(s, temp);
      return;
    case 0x80: // bra
      branch(s, get_instruction_byte(s));
      return;

    case 0x4c: // jmp
      temp = get_instruction_byte(s);
      temp = (get_instruction_byte(s)<<8) | temp;
      s->pc = temp;
      return;
    case 0x6c: // jmp indirect
      temp = get_instruction_byte(s);
      temp = (get_instruction_byte(s)<<8) | temp;
      address = s->read(s->hardware, temp);
      address = (s->read(s->hardware, temp+1)<<8) | address;
      s->pc = temp;
      return;
    case 0x7c: // jmp indirect indexed
      temp = get_instruction_byte(s);
      temp = (get_instruction_byte(s)<<8) | temp;
      address = s->read(s->hardware, temp+s->x);
      address = (s->read(s->hardware, temp+1+s->x)<<8) | address;
      s->pc = temp;
      return;

    case 0x20: // jsr
      temp = get_instruction_byte(s);
      temp = (get_instruction_byte(s)<<8) | temp;
      push(s, (s->pc-1)>>8);  // high
      push(s, (s->pc-1)&255); // low
      s->pc = temp;
      return;
    case 0x40: // rti
      s->flags = pop(s);
      temp = pop(s);
      temp = (pop(s)<<8) | temp;
      s->pc = temp;
      return;


    case 0x60: // rts
      temp = pop(s);
      temp = (pop(s)<<8) | temp;
      s->pc = temp+1;
      return;

    case 0xA0: // ldy imm
      op_ldy(s, get_instruction_byte(s));
      return;
    case 0xC0: // cpy imm
      op_cpy(s, get_instruction_byte(s));
      return;

    case 0xE0: // cpx imm
      op_cpx(s, get_instruction_byte(s));
      return;

    case 0x89: // bit imm
      op_bit(s, get_instruction_byte(s));
      return;
    case 0x24: // bit zp
      op_bit(s, s->read(s->hardware, zeropage(s)));
      return;
    case 0x34: // bit zp,x
      op_bit(s, s->read(s->hardware, zeropage_x(s)));
      return;
    case 0x2c: // bit abs
      op_bit(s, s->read(s->hardware, absolute(s)));
      return;
    case 0x3c: // bit abs,x
      op_bit(s, s->read(s->hardware, absolute_x(s)));
      return;

    case 0x1a: // ina
      s->a++;
      update_nz(s, s->a);
      return;
    case 0x3a: // dea
      s->a--;
      update_nz(s, s->a);
      return;

    case 0xe8: // inx
      s->x++;
      update_nz(s, s->x);
      return;
    case 0xca: // dex
      s->x--;
      update_nz(s, s->x);
      return;

    case 0xc8: // iny
      s->y++;
      update_nz(s, s->y);
      return;
    case 0x88: // dey
      s->y--;
      update_nz(s, s->y);
      return;

    case 0x5a: // phy
      push(s, s->y);
      return;
    case 0x6a: // ply
      s->y = pop(s);
      return;

    case 0xda: // phx
      push(s, s->x);
      return;
    case 0xfa: // plx
      s->x = pop(s);
      return;
    case 0x08: // php
      push(s, s->flags | 16);
      return;
    case 0x28: // plp
      s->flags = pop(s);
      return;

    case 0x48: // pha
      push(s, s->a | 16);
      return;
    case 0x68: // pla
      s->a = pop(s);
      return;

    case 0x58: // cli
      s->flags &= ~FLAG_NO_IRQ;
      return;
    case 0x78: // sei
      s->flags |= FLAG_NO_IRQ;
      return;

    case 0x18: // clc
      s->flags &= ~FLAG_CARRY;
      return;
    case 0x38: // sec
      s->flags |= FLAG_CARRY;
      return;

    case 0x9a: // txs
      s->s = s->x;
      return;
    case 0xba: // tsx
      op_ldx(s, s->s);
      return;


    case 0x98: // tya
      op_lda(s, s->y);
      return;
    case 0xa8: // tay
      op_ldy(s, s->a);
      return;

    case 0xb8: // clv
      s->flags &= ~FLAG_OVERFLOW;
      return;

    case 0xd8: // cld
      s->flags &= ~FLAG_DECIMAL;
      return;
    case 0xf8: // sed
      s->flags |= FLAG_DECIMAL;
      return;

    case 0xea: // nop
      return;

  }

  // main instructions
  if(cc == 1) {
    static const address_mode modes[] = {
      indirect_x,
      zeropage,
      NULL,
      absolute,
      indirect_y,
      zeropage_x,
      absolute_y,
      absolute_x
    };

    if(aaa == 4) { // STA
      address = modes[bbb](s);
      s->write(s->hardware, address, s->a);
      return;
    } else {      // not STA
      if(bbb == 2) { // immediate
        value = get_instruction_byte(s);
      } else { // memory
        value = s->read(s->hardware, modes[bbb](s));
      }

      main_ops[aaa](s, value);
    }

  } else if(cc == 2) { // mostly read-modify-write
    if(bbb == 4) { // indirect zeropage
      if(aaa == 4) { // STA
        s->write(s->hardware, indirect(s), s->a);
      } else {      // not STA
        value = s->read(s->hardware, indirect(s));
        main_ops[aaa](s, value);
      }
      return;
    }

    switch(opcode) {
      // ldx
      case 0xa2:
        op_ldx(s, get_instruction_byte(s));
        return;
      case 0xa6:
        op_ldx(s, s->read(s->hardware, zeropage(s)));
        return;
      case 0xae:
        op_ldx(s, s->read(s->hardware, absolute(s)));
        return;
      case 0xb6:
        op_ldx(s, s->read(s->hardware, zeropage_y(s)));
        return;
      case 0xbe:
        op_ldx(s, s->read(s->hardware, absolute_y(s)));
        return;
      case 0xaa:
        op_ldx(s, s->a);
        return;
      // stx
      case 0x86:
        s->write(s, zeropage(s), s->x);
        return;
      case 0x8e:
        s->write(s, absolute(s), s->x);
        return;
      case 0x96:
        s->write(s, zeropage(s), s->x);
        return;
      case 0x8a:
        op_lda(s, s->x);
        return;
    }

    switch(bbb) {
      case 1:
        address = zeropage(s);
        value = s->read(s->hardware, address);
        break;
      case 2:
        value = s->a;
        break;
      case 3:
        address = absolute(s);
        value = s->read(s->hardware, address);
        break;
      case 5:
        address = zeropage_x(s);
        value = s->read(s->hardware, address);
        break;
      case 7:
        address = absolute_x(s);
        value = s->read(s->hardware, address);
        break;
    }

    switch(aaa) {
      case 0: // asl
        s->flags &= ~FLAG_CARRY;
        if(value & 0x80)
          s->flags |= FLAG_CARRY;
        value <<= 1;
        break;
      case 1: // rol
        temp = s->flags & FLAG_CARRY;
        s->flags &= ~FLAG_CARRY;
        if(value & 0x80)
          s->flags |= FLAG_CARRY;
        value = (value << 1) | (temp?1:0);
        break;
      case 2: // lsr
        s->flags &= ~FLAG_CARRY;
        if(value & 0x01)
          s->flags |= FLAG_CARRY;
        value >>= 1;
        break;
      case 3: // ror
        temp = s->flags & FLAG_CARRY;
        s->flags &= ~FLAG_CARRY;
        if(value & 0x01)
          s->flags |= FLAG_CARRY;
        value = (value >> 1) | (temp?0x80:0);
        break;
      case 6: // dec
        value--;
        break;
      case 7: // inc
        value++;
        break;
    }
    update_nz(s, value);

    switch(bbb) {
      case 1:
      case 3:
      case 5:
      case 7:
        s->write(s->hardware, address, value);
        break;
    }

  } else if(cc == 0) {

  } else if(cc == 3) {
    int bit = (opcode >> 4) & 3;
    if((opcode & 7) == 7) { // zeropage bit instructions
      address = zeropage(s);
      if(opcode & 8) { // test-and-branch
        temp = get_instruction_byte(s);
        if(opcode & 128) {
          if(s->read(s->hardware, address) & (1 << bit))
            branch(s, temp);
        } else {
          if(!(s->read(s->hardware, address) & (1 << bit)))
            branch(s, temp);
        }
      } else { // bit set/reset
        if(opcode & 128) {
          s->write(s->hardware, address, s->read(s->hardware, address) | (1 << bit));
        } else {
          s->write(s->hardware, address, s->read(s->hardware, address) & ~(1 << bit));
        }
      }
    }
  }

}
