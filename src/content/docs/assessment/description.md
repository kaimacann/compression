---
title: Description
description: The project brief, development process, checkpoints, and example projects.
---

NOTE: In addition to the above marking criteria, it is mandatory for each group to pass 3 checkpoints as outlined in the "Expected project timeline, in-class activities and checkpoints" section.

The Rubric used for marking each deliverable is shown below.

## C source code

You will get a mark out of three levels for each of the following:

| Criterion | Level 1 (1/3) | Level 2 (2/3) | Level 3 (3/3) |
| --- | --- | --- | --- |
| Coding style and modularity of the source code | Below par effort. | Reasonably good habits. Effort to break the program into functions, or effort to implement a user defined C library. | All good habits listed in LAB01. C library with protection against double inclusion has been implemented with appropriate features broken into appropriate files. A makefile is provided. |
| Features and aesthetics | Some reasonably functional features with operable user interface. | Very useful features with a good operable user interface. | Features are useful and challenging to implement. User interface is very intuitive. The program has multiple runtime modes that use command line arguments. |
| Data compression/ encryption techniques, and other algorithms | Some effort to understand and use relevant compression/encryption concepts. | Reasonably complex compression/encryption algorithms have been implemented. Some effort to use advanced data structures. | Implement - reasonably complex compression/encryption algorithms and other advanced algorithms such as sorting and searching. Shows advanced use of data structures such as Stack, Queue, Linked List and Tree. |
| Bugs | Some bugs. | No bugs are evident. Works for inputs that are outside the expected range but of the expected data types. Provides a debug mode (with preprocessing directives) which prints useful debug information on the terminal. | No bugs are evident. Provides a debug mode (with preprocessing directives) which prints useful debug information on the terminal. Works even for the inputs that are outside the expected range and not the expected data types. |

The sum of above marks will produce 15/30 of the marks for this assignment (15% of the grade in this subject).

*Compression means your code can do compression and decompression. Encryption means your code can do encryption and decription.

## Project report

You will get a mark out of three levels for each of the following:

| Criterion | Level 1 (1/3) | Level 2 (2/3) | Level 3 (3/3) |
| --- | --- | --- | --- |
| Objective, scope and program features | Objective of the program explained and the scope of the project is reasonably sufficient. Some features have been explained. | Clear objective. Scope is sufficient for a group project. Reasonable discussion on features. | Very clear and interesting objective. Scope is broad and challenging to achieve. Good discussion on features implemented. |
| Explaining design | Some information on the design, mostly repeating comments from the source code. | Shows good understanding and reasoning behind the design of choice. Discuss how the features were implemented. | Shows deep understanding and reasoning behind the design of choice. Discusses why the chosen design is the best (compare to alternatives). Identify the interdependencies through a structured analysis (provide structure charts or flow charts). [https://en.wikipedia.org/wiki/Structured_analysis](https://en.wikipedia.org/wiki/Structured_analysis) |
| Critical thinking | Some discussion on problems encountered and how they were overcome. | Detailed discussion on problems encountered and how they were overcome. | Detailed discussion on problems encountered, how they were overcome, alternatives considered and why the alternatives did/would not have given the desired outcome. |
| Presentation | Readable. Some effort to break the text to paragraphs and sections. | Easy to follow. Broken into appropriate paragraphs and sections. Reasonable level of graphical illustrations. Incomplete effort to cite sources. | Very easy to follow. Broken into appropriate paragraphs and sections. Detailed and highly effective graphical illustrations. Sources are cited correctly. [https://lib.uts.edu.au/sites/default/files/attachments/page/InteractiveHarvardUTSGuide.pdf](https://lib.uts.edu.au/sites/default/files/attachments/page/InteractiveHarvardUTSGuide.pdf) |

The sum of above marks will produce 15/30 of the marks for this assignment (15% of the grade in this subject).

## Feedback

You will receive the following feedback within three weeks of the due date: individual marks for each deliverable adjusted according to peer review and the rubric for the group.

## Request to review

If you are confident that there are substantial irregularities in your marks, you may personally approach the tutors, not the convenor, within five days after results are released on Canvas to lodge a request to review. A request to review could result in marks being left unchanged or decreased if it is found that the marking was too lenient.

## Plagiarism

Plagiarism is taken very seriously.

- Plagiarism detection software will be used to check that you did not copy parts of the solution from previous submissions, other students in the class, online forums, or other online resources.
- You could receive zero marks for the assignment or fail the unit if you plagiarise.
- If one student in the group commits plagiarism in this group assessment, all group members may be subject to the formal misconduct procedure of the University.

## Attendance

By attending the lab you will be able to meet and discuss with group members. You will also benefit from discussing your project with your tutor. Attendance in each lab is mandatory. Attendance will be recorded and reflected in the peer-review adjustment of individual marks.

## Description

In this research-inspired project, students will work together in teams to develop a professional-grade program that manipulates real-world data. You will develop teamwork, project management, and communication skills.

## Group formation

Students are required to form their own groups. The typical group size is three students per group.

- Once you have formed your group, obtain a group name from your tutor. Group names must follow the format `Day_Time_GroupNumber`, for example `Wed_4pm_01`.
- After receiving the group name, one member should create the group on Canvas via **People → Project Group → + Group**, and all group members must join the group.
- It is your responsibility to ensure that all members are correctly enrolled in the same group on Canvas.
- If you are unable to form a group, contact your tutor, who will assist you in forming or assigning a group.

> **Allowed libraries:** You may use only the standard C libraries `stdio`, `stdlib`, `string`, and `math`. You may develop your own libraries, but you must not use other libraries that come with GCC or libraries developed by other programmers. You may receive zero marks for the source code if you fail to follow this requirement.

## The project brief

Put yourself in the position of a C programmer working in a company as a team. Your task is to find a customer with a real-world data manipulation need that involves data compression and/or encryption. Your customer could be someone from industry, your parents or siblings, a friend, or yourself. The customer requirement must have at least one component of compression or encryption.

If you are trying to pass, choose either a compression or encryption problem. If you are trying to achieve a high mark, consider doing both. For an even higher mark, implement additional algorithms such as sorting and searching as specified in the marking criteria.

Once you find a customer and an appropriate problem, work together as a team to develop a C program that solves the problem. Break the task into small functions and assign functions to team members. Team members can develop at home and use laboratory time to integrate the code.

If you are only trying to pass, it is sufficient to integrate the code from all team members into one program that can be compiled as in the semester exercises. For a high mark, develop your own library as a header file, have different runtime modes using command-line parsing, and develop makefiles. You may also provide a debugging mode using preprocessor directives.

For the highest marks, demonstrate advanced data structures such as linked lists and queues as specified in the marking criteria. Students only trying to pass can use simple data structures such as structs and arrays.

You will go through the software development cycle as follows:

### Problem requirement

Meet potential clients and find a real-world data manipulation need that involves data compression and/or encryption.

### Problem analysis

Identify the inputs, outputs, and required resources. Break the data manipulation task into small functions. Think about features you want to add to the program. Start writing parts of the project report and read the submission requirements to understand what should be included.

### Solution design

Develop simple algorithms to achieve the objectives of each function. Develop the program design — single file or custom C library — and consider a reasonably user-friendly interface.

### Implement and test

Research and learn simple encryption and/or compression algorithms. Implement one function at a time and test every function as you develop it. Complete the project report.

> **This assessment is open ended:** You will need to find an appropriate problem yourself. The teaching team specifies the rules, but not the exact project. How far you push the project and how good the final program is are up to you within the allocated time.

Being able to develop is not enough. You need to communicate what you did and how you did it in the project report, as you would in industry.

The focus is to showcase the knowledge gathered through the subject by applying it to a real-world data encryption and/or compression problem. Depending on the mark you are aiming for, you may need to research compression or encryption algorithms, data structures, and related topics to meet the criteria.

## Your approach

Developing a program as a team is different from writing your own program. Working as a team helps develop large programs, but success depends on having a proper plan from day zero and the ability to communicate.

### Tips

1. Take time to get to know your teammates and establish clear communication from the start. Make sure everyone is comfortable sharing ideas, asking questions, and providing updates regularly.
2. When writing a program by yourself, you might write the code first and comments afterwards. When writing as a team, it can be more effective to write the comments first and then the code.
3. Group project does not mean one person writes the code while another writes the report. Everyone should contribute to all deliverables.
4. Differentiate the **what** from the **how**. Spend laboratory time figuring out what to build as a team, and worry about how to build it as individuals at home.
5. Integration is a large task when working as a group. Develop at home individually and integrate as a team during laboratory time.

### Where to get help

- Tutors can help you understand the task. Make full use of them.
- Forum 7 covers the linked-list data structure.
- Forum 8 covers writing your own libraries and makefiles.
- Forum 9 covers command-line parsing for implementing different runtime modes.
- See the example project section below for encryption and compression resources.
- Advanced sorting/searching algorithms and handling incorrect input data types are required for very high marks and need self-driven research.

## Expected project timeline, in-class activities and checkpoints

### Week 08

- Go through the description of Assignment 3.
- Group members discuss and choose a problem to work on.
- Think about function prototypes and the report outline.

### Week 09

Before the lab:

- Write all function prototypes and block comments for the problem.
- Produce a structure chart.
- Produce the project report outline with section headings and fill in the scope.

During the lab, each group may ask tutors for feedback.

**Checkpoint 1:** Send the report outline with the scope to your tutor. Other sections may be incomplete. Submit one or more `.c`/`.h` files with function prototypes, block comments, and the name of the author of each function on Ed. This checkpoint is mandatory. Submission is through Canvas for the report outline and Ed for the source code.

### Week 10

- Each student comes to the lab having implemented functions and algorithms.
- Debug and integrate the code.
- Readjust the scope and make necessary changes to the program design based on tutor feedback.

### Week 11

- Implement, debug, and integrate the code.
- Document bugs found, how they were found, and the steps taken to fix them.
- Fill in more sections of the project report.

**Checkpoint 2:** Demonstrate a running, though not necessarily complete, program to the tutor in the lab. This checkpoint is mandatory.

### Week 12

- Ensure the final program is functional and the coding style is good.
- Finalise the project report.

**Checkpoint 3:** Demonstrate the complete program to the tutor in the lab. This checkpoint is mandatory.

## Submission of deliverables

- Confirm the final submission within the group.
- Submit the SPARK review, score, and comments. Only the scores and comments for the source code and report will be considered.
- Leave the presentation section in SPARK as 100%; this section is not included in the SPA calculation.
- Submit source code on Ed and the report on Canvas.

## Example project — compact secure disk

Develop a C program that works as a compact secure disk, allowing users to store compressed and password-protected files in one directory. The user may pass a document to the program, which encrypts the document using a master password. The encrypted document is then compressed and stored in a directory. When required, the user can decompress and decrypt the files.

The program may batch process tasks using command-line arguments and/or work interactively. It may also include an advanced search feature that looks for a specific keyword within an encrypted and compressed file.

This is only one example. Students are encouraged to find their own compression and/or encryption problems.

### Compression techniques to consider

- [Run-length encoding](https://en.wikipedia.org/wiki/Run-length_encoding) is relatively easy but sufficient to meet the compression requirement.
- [Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding) is more difficult and suitable for experienced programmers.

### Encryption techniques to consider

- [Substitution cipher](https://en.wikipedia.org/wiki/Substitution_cipher).
- [XOR cipher](https://en.wikipedia.org/wiki/XOR_cipher).

You could create your own compression and/or encryption algorithm. You could also implement multiple algorithms and allow the user to choose one.

> **You are free to add features:** Unlike the individual assignment, this group project is open ended. You are free to add features as you see fit, and you will not receive a template source file.

## Example project — image encryption program

Develop a C program that maintains a repository of images encrypted using a password. The program is capable of decrypting images correctly when the user enters the correct password. It takes a BMP image and produces an encrypted version.

![Original image and encrypted image](/images/group-project-image-encryption.svg)

The program can maintain a list of recently encrypted image filenames using a queue. It can search through all files in the image repository, allowing the user to enter part of a filename and search for matching files.

The program may also have a quiet mode. In quiet, batch mode, the user can encrypt several images by typing filenames directly into the command line separated by spaces. The program does not prompt for input while it is running, but displays progress messages.

This is another example. You are encouraged to find your own customer requirements, although you may be inspired by the examples above.
