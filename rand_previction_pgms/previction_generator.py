import random

def random_register():
    return f'r{random.randint(0, 30)}'

def random_immediate():
    return f'#{random.randint(0, 255)}'

def random_ldr_operands():
    return f'{random_register()}, [{random_register()}, {random_immediate()}]'

def random_label(labels):
    return random.choice(labels)

def random_instruction(labels):
    instructions = [
        f'add {random_register()}, {random_register()}, {random_register()}',
        f'sub {random_register()}, {random_register()}, {random_register()}',
        f'mul {random_register()}, {random_register()}, {random_register()}',
        f'mov {random_register()}, {random_immediate()}',
        f'b {random_label(labels)}',
        f'bl {random_label(labels)}',
        f'beq {random_label(labels)}',
        f'bne {random_label(labels)}',
        f'bge {random_label(labels)}',
        f'blt {random_label(labels)}',
        f'bgt {random_label(labels)}',
        f'ble {random_label(labels)}',
    ]
    return random.choice(instructions)

def generate_program():
    program = []
    labels = [f'label{i}' for i in range(5)]

    # Add labels to the program
    for label in labels:
        program.append(f'{label}:')

    # Add 3 random instructions
    for _ in range(3):
        program.append(random_instruction(labels))

    # Add LDR instruction
    program.append(f'ldr {random_ldr_operands()}')

    # Add 0-4 random instructions
    for _ in range(random.randint(0, 4)):
        program.append(random_instruction(labels))

    # Add 4 LDR instructions
    for _ in range(4):
        program.append(f'ldr {random_ldr_operands()}')

    # Add 0-4 random instructions
    for _ in range(random.randint(0, 4)):
        program.append(random_instruction(labels))

    return program

def main():
    program = generate_program()
    for line in program:
        print(line)

if __name__ == '__main__':
    main()
