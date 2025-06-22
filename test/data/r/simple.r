node_id,type,name,file_path,language,start_line,start_column,end_line,end_column,parent_id,depth,sibling_index,children_count,descendant_count,peek,semantic_type,universal_flags,arity_bin
0,program,,test/data/r/simple.R,r,1,1,69,32,,0,0,35,380,"# Simple R test file for AST parsing
# This file contains basic R constructs for testing

# Variable assignments
x <- 5
",191,0,7
1,comment,,test/data/r/simple.R,r,1,1,1,37,0,1,0,0,0,"# Simple R test file for AST parsing",32,0,0
2,comment,,test/data/r/simple.R,r,2,1,2,52,0,1,1,0,0,"# This file contains basic R constructs for testing",32,0,0
3,comment,,test/data/r/simple.R,r,4,1,4,23,0,1,2,0,0,"# Variable assignments",32,0,0
4,binary_operator,,test/data/r/simple.R,r,5,1,5,7,0,1,3,3,3,x <- 5,192,0,3
5,identifier,x,test/data/r/simple.R,r,5,1,5,2,4,2,0,0,0,x,80,0,0
6,<-,,test/data/r/simple.R,r,5,3,5,5,4,2,1,0,0,<-,204,0,0
7,float,5,test/data/r/simple.R,r,5,6,5,7,4,2,2,0,0,5,65,0,0
8,binary_operator,,test/data/r/simple.R,r,6,1,6,7,0,1,4,3,3,y = 10,192,0,3
9,identifier,y,test/data/r/simple.R,r,6,1,6,2,8,2,0,0,0,y,80,0,0
10,=,,test/data/r/simple.R,r,6,3,6,4,8,2,1,0,0,=,204,0,0
11,float,10,test/data/r/simple.R,r,6,5,6,7,8,2,2,0,0,10,65,0,0
12,binary_operator,,test/data/r/simple.R,r,7,1,7,9,0,1,5,3,3,z <<- 15,192,0,3
13,identifier,z,test/data/r/simple.R,r,7,1,7,2,12,2,0,0,0,z,80,0,0
14,<<-,,test/data/r/simple.R,r,7,3,7,6,12,2,1,0,0,<<-,204,0,0
15,float,15,test/data/r/simple.R,r,7,7,7,9,12,2,2,0,0,15,65,0,0
16,comment,,test/data/r/simple.R,r,9,1,9,22,0,1,6,0,0,"# Function definition",32,0,0
17,binary_operator,,test/data/r/simple.R,r,10,1,15,2,0,1,7,3,65,"calculate_mean <- function(values, na.rm = TRUE) {
    if (na.rm) {
        values <- values[!is.na(values)]
    }
    r",192,0,3
18,identifier,calculate_mean,test/data/r/simple.R,r,10,1,10,15,17,2,0,0,0,calculate_mean,80,0,0
19,<-,,test/data/r/simple.R,r,10,16,10,18,17,2,1,0,0,<-,204,0,0
20,function_definition,calculate_mean,test/data/r/simple.R,r,10,19,15,2,17,2,2,3,62,"function(values, na.rm = TRUE) {
    if (na.rm) {
        values <- values[!is.na(values)]
    }
    return(sum(values) ",240,0,3
21,function,,test/data/r/simple.R,r,10,19,10,27,20,3,0,0,0,function,240,1,0
22,parameters,,test/data/r/simple.R,r,10,27,10,49,20,3,1,5,9,"(values, na.rm = TRUE)",181,0,4
23,(,,test/data/r/simple.R,r,10,27,10,28,22,4,0,0,0,(,4,0,0
24,parameter,values,test/data/r/simple.R,r,10,28,10,34,22,4,1,1,1,values,246,0,1
25,identifier,values,test/data/r/simple.R,r,10,28,10,34,24,5,0,0,0,values,80,0,0
26,comma,,test/data/r/simple.R,r,10,34,10,35,22,4,2,0,0,",",8,0,0
27,parameter,na.rm,test/data/r/simple.R,r,10,36,10,48,22,4,3,3,3,na.rm = TRUE,246,0,3
28,identifier,na.rm,test/data/r/simple.R,r,10,36,10,41,27,5,0,0,0,na.rm,80,0,0
29,=,,test/data/r/simple.R,r,10,42,10,43,27,5,1,0,0,=,204,0,0
30,true,TRUE,test/data/r/simple.R,r,10,44,10,48,27,5,2,0,0,TRUE,72,0,0
31,),,test/data/r/simple.R,r,10,48,10,49,22,4,4,0,0,),4,0,0
32,braced_expression,,test/data/r/simple.R,r,10,50,15,2,20,3,2,4,50,"{
    if (na.rm) {
        values <- values[!is.na(values)]
    }
    return(sum(values) / length(values))
}",176,0,4
33,{,,test/data/r/simple.R,r,10,50,10,51,32,4,0,0,0,{,4,0,0
34,if_statement,,test/data/r/simple.R,r,11,5,13,6,32,4,1,5,25,"if (na.rm) {
        values <- values[!is.na(values)]
    }",144,0,4
35,if,,test/data/r/simple.R,r,11,5,11,7,34,5,0,0,0,if,144,1,0
36,(,,test/data/r/simple.R,r,11,8,11,9,34,5,1,0,0,(,4,0,0
37,identifier,na.rm,test/data/r/simple.R,r,11,9,11,14,34,5,2,0,0,na.rm,80,0,0
38,),,test/data/r/simple.R,r,11,14,11,15,34,5,3,0,0,),4,0,0
39,braced_expression,,test/data/r/simple.R,r,11,16,13,6,34,5,4,3,20,"{
        values <- values[!is.na(values)]
    }",176,0,3
40,{,,test/data/r/simple.R,r,11,16,11,17,39,6,0,0,0,{,4,0,0
41,binary_operator,,test/data/r/simple.R,r,12,9,12,41,39,6,1,3,17,values <- values[!is.na(values)],192,0,3
42,identifier,values,test/data/r/simple.R,r,12,9,12,15,41,7,0,0,0,values,80,0,0
43,<-,,test/data/r/simple.R,r,12,16,12,18,41,7,1,0,0,<-,204,0,0
44,subset,,test/data/r/simple.R,r,12,19,12,41,41,7,2,2,14,values[!is.na(values)],212,0,2
45,identifier,values,test/data/r/simple.R,r,12,19,12,25,44,8,0,0,0,values,80,0,0
46,arguments,,test/data/r/simple.R,r,12,25,12,41,44,8,1,3,12,[!is.na(values)],181,0,3
47,[,,test/data/r/simple.R,r,12,25,12,26,46,9,0,0,0,[,4,0,0
48,argument,,test/data/r/simple.R,r,12,26,12,40,46,9,1,1,9,!is.na(values),180,0,1
49,unary_operator,,test/data/r/simple.R,r,12,26,12,40,48,10,0,2,8,!is.na(values),192,0,2
50,!,,test/data/r/simple.R,r,12,26,12,27,49,11,0,0,0,!,196,0,0
51,call,is.na,test/data/r/simple.R,r,12,27,12,40,49,11,1,2,6,is.na(values),208,0,2
52,identifier,is.na,test/data/r/simple.R,r,12,27,12,32,51,12,0,0,0,is.na,80,0,0
53,arguments,,test/data/r/simple.R,r,12,32,12,40,51,12,1,3,4,(values),181,0,3
54,(,,test/data/r/simple.R,r,12,32,12,33,53,13,0,0,0,(,4,0,0
55,argument,values,test/data/r/simple.R,r,12,33,12,39,53,13,1,1,1,values,180,0,1
56,identifier,values,test/data/r/simple.R,r,12,33,12,39,55,14,0,0,0,values,80,0,0
57,),,test/data/r/simple.R,r,12,39,12,40,53,13,2,0,0,),4,0,0
58,],,test/data/r/simple.R,r,12,40,12,41,46,9,2,0,0,],4,0,0
59,},,test/data/r/simple.R,r,13,5,13,6,39,6,2,0,0,},4,0,0
60,call,,test/data/r/simple.R,r,14,5,14,41,32,4,2,2,21,return(sum(values) / length(values)),208,0,2
61,return,,test/data/r/simple.R,r,14,5,14,11,60,5,0,0,0,return,152,1,0
62,arguments,,test/data/r/simple.R,r,14,11,14,41,60,5,1,3,19,(sum(values) / length(values)),181,0,3
63,(,,test/data/r/simple.R,r,14,11,14,12,62,6,0,0,0,(,4,0,0
64,argument,,test/data/r/simple.R,r,14,12,14,40,62,6,1,1,16,sum(values) / length(values),180,0,1
65,binary_operator,,test/data/r/simple.R,r,14,12,14,40,64,7,0,3,15,sum(values) / length(values),192,0,3
66,call,sum,test/data/r/simple.R,r,14,12,14,23,65,8,0,2,6,sum(values),208,0,2
67,identifier,sum,test/data/r/simple.R,r,14,12,14,15,66,9,0,0,0,sum,80,0,0
68,arguments,,test/data/r/simple.R,r,14,15,14,23,66,9,1,3,4,(values),181,0,3
69,(,,test/data/r/simple.R,r,14,15,14,16,68,10,0,0,0,(,4,0,0
70,argument,values,test/data/r/simple.R,r,14,16,14,22,68,10,1,1,1,values,180,0,1
71,identifier,values,test/data/r/simple.R,r,14,16,14,22,70,11,0,0,0,values,80,0,0
72,),,test/data/r/simple.R,r,14,22,14,23,68,10,2,0,0,),4,0,0
73,/,,test/data/r/simple.R,r,14,24,14,25,65,8,1,0,0,/,192,0,0
74,call,length,test/data/r/simple.R,r,14,26,14,40,65,8,2,2,6,length(values),208,0,2
75,identifier,length,test/data/r/simple.R,r,14,26,14,32,74,9,0,0,0,length,80,0,0
76,arguments,,test/data/r/simple.R,r,14,32,14,40,74,9,1,3,4,(values),181,0,3
77,(,,test/data/r/simple.R,r,14,32,14,33,76,10,0,0,0,(,4,0,0
78,argument,values,test/data/r/simple.R,r,14,33,14,39,76,10,1,1,1,values,180,0,1
79,identifier,values,test/data/r/simple.R,r,14,33,14,39,78,11,0,0,0,values,80,0,0
80,),,test/data/r/simple.R,r,14,39,14,40,76,10,2,0,0,),4,0,0
81,),,test/data/r/simple.R,r,14,40,14,41,62,6,2,0,0,),4,0,0
82,},,test/data/r/simple.R,r,15,1,15,2,32,4,3,0,0,},4,0,0
83,comment,,test/data/r/simple.R,r,17,1,17,18,0,1,8,0,0,"# Data structures",32,0,0
84,binary_operator,,test/data/r/simple.R,r,18,1,18,30,0,1,9,3,21,"my_vector <- c(1, 2, 3, 4, 5)",192,0,3
85,identifier,my_vector,test/data/r/simple.R,r,18,1,18,10,84,2,0,0,0,my_vector,80,0,0
86,<-,,test/data/r/simple.R,r,18,11,18,13,84,2,1,0,0,<-,204,0,0
87,call,c,test/data/r/simple.R,r,18,14,18,30,84,2,2,2,18,"c(1, 2, 3, 4, 5)",208,0,2
88,identifier,c,test/data/r/simple.R,r,18,14,18,15,87,3,0,0,0,c,80,0,0
89,arguments,,test/data/r/simple.R,r,18,15,18,30,87,3,1,11,16,"(1, 2, 3, 4, 5)",181,0,6
90,(,,test/data/r/simple.R,r,18,15,18,16,89,4,0,0,0,(,4,0,0
91,argument,,test/data/r/simple.R,r,18,16,18,17,89,4,1,1,1,1,180,0,1
92,float,1,test/data/r/simple.R,r,18,16,18,17,91,5,0,0,0,1,65,0,0
93,comma,,test/data/r/simple.R,r,18,17,18,18,89,4,2,0,0,",",8,0,0
94,argument,,test/data/r/simple.R,r,18,19,18,20,89,4,3,1,1,2,180,0,1
95,float,2,test/data/r/simple.R,r,18,19,18,20,94,5,0,0,0,2,65,0,0
96,comma,,test/data/r/simple.R,r,18,20,18,21,89,4,4,0,0,",",8,0,0
97,argument,,test/data/r/simple.R,r,18,22,18,23,89,4,5,1,1,3,180,0,1
98,float,3,test/data/r/simple.R,r,18,22,18,23,97,5,0,0,0,3,65,0,0
99,comma,,test/data/r/simple.R,r,18,23,18,24,89,4,6,0,0,",",8,0,0
100,argument,,test/data/r/simple.R,r,18,25,18,26,89,4,7,1,1,4,180,0,1
101,float,4,test/data/r/simple.R,r,18,25,18,26,100,5,0,0,0,4,65,0,0
102,comma,,test/data/r/simple.R,r,18,26,18,27,89,4,8,0,0,",",8,0,0
103,argument,,test/data/r/simple.R,r,18,28,18,29,89,4,9,1,1,5,180,0,1
104,float,5,test/data/r/simple.R,r,18,28,18,29,103,5,0,0,0,5,65,0,0
105,),,test/data/r/simple.R,r,18,29,18,30,89,4,10,0,0,),4,0,0
106,binary_operator,,test/data/r/simple.R,r,19,1,23,2,0,1,10,3,24,"my_list <- list(
    numbers = my_vector,
    text = ""hello world"",
    logical = TRUE
)",192,0,3
107,identifier,my_list,test/data/r/simple.R,r,19,1,19,8,106,2,0,0,0,my_list,80,0,0
108,<-,,test/data/r/simple.R,r,19,9,19,11,106,2,1,0,0,<-,204,0,0
109,call,list,test/data/r/simple.R,r,19,12,23,2,106,2,2,2,21,"list(
    numbers = my_vector,
    text = ""hello world"",
    logical = TRUE
)",208,0,2
110,identifier,list,test/data/r/simple.R,r,19,12,19,16,109,3,0,0,0,list,80,0,0
111,arguments,,test/data/r/simple.R,r,19,16,23,2,109,3,1,7,19,"(
    numbers = my_vector,
    text = ""hello world"",
    logical = TRUE
)",181,0,5
112,(,,test/data/r/simple.R,r,19,16,19,17,111,4,0,0,0,(,4,0,0
113,argument,numbers,test/data/r/simple.R,r,20,5,20,24,111,4,1,3,3,numbers = my_vector,180,0,3
114,identifier,numbers,test/data/r/simple.R,r,20,5,20,12,113,5,0,0,0,numbers,80,0,0
115,=,,test/data/r/simple.R,r,20,13,20,14,113,5,1,0,0,=,204,0,0
116,identifier,my_vector,test/data/r/simple.R,r,20,15,20,24,113,5,2,0,0,my_vector,80,0,0
117,comma,,test/data/r/simple.R,r,20,24,20,25,111,4,2,0,0,",",8,0,0
118,argument,text,test/data/r/simple.R,r,21,5,21,25,111,4,3,3,6,"text = ""hello world""",180,0,3
119,identifier,text,test/data/r/simple.R,r,21,5,21,9,118,5,0,0,0,text,80,0,0
120,=,,test/data/r/simple.R,r,21,10,21,11,118,5,1,0,0,=,204,0,0
121,string,"""hello world""",test/data/r/simple.R,r,21,12,21,25,118,5,2,3,3,"""hello world""",68,0,3
122,"""",,test/data/r/simple.R,r,21,12,21,13,121,6,0,0,0,"""",8,0,0
123,string_content,,test/data/r/simple.R,r,21,13,21,24,121,6,1,0,0,hello world,68,0,0
124,"""",,test/data/r/simple.R,r,21,24,21,25,121,6,2,0,0,"""",8,0,0
125,comma,,test/data/r/simple.R,r,21,25,21,26,111,4,4,0,0,",",8,0,0
126,argument,logical,test/data/r/simple.R,r,22,5,22,19,111,4,5,3,3,logical = TRUE,180,0,3
127,identifier,logical,test/data/r/simple.R,r,22,5,22,12,126,5,0,0,0,logical,80,0,0
128,=,,test/data/r/simple.R,r,22,13,22,14,126,5,1,0,0,=,204,0,0
129,true,TRUE,test/data/r/simple.R,r,22,15,22,19,126,5,2,0,0,TRUE,72,0,0
130,),,test/data/r/simple.R,r,23,1,23,2,111,4,6,0,0,),4,0,0
131,comment,,test/data/r/simple.R,r,25,1,25,15,0,1,11,0,0,"# Control flow",32,0,0
132,for_statement,,test/data/r/simple.R,r,26,1,32,2,0,1,12,7,68,"for (i in 1:10) {
    if (i %% 2 == 0) {
        print(paste(""Even:"", i))
    } else {
        print(paste(""Odd:"", i))
 ",149,0,5
133,for,,test/data/r/simple.R,r,26,1,26,4,132,2,0,0,0,for,148,1,0
134,(,,test/data/r/simple.R,r,26,5,26,6,132,2,1,0,0,(,4,0,0
135,identifier,i,test/data/r/simple.R,r,26,6,26,7,132,2,2,0,0,i,80,0,0
136,in,,test/data/r/simple.R,r,26,8,26,10,132,2,3,0,0,in,148,1,0
137,binary_operator,,test/data/r/simple.R,r,26,11,26,15,132,2,4,3,3,1:10,192,0,3
138,float,1,test/data/r/simple.R,r,26,11,26,12,137,3,0,0,0,1,65,0,0
139,:,,test/data/r/simple.R,r,26,12,26,13,137,3,1,0,0,:,192,0,0
140,float,10,test/data/r/simple.R,r,26,13,26,15,137,3,2,0,0,10,65,0,0
141,),,test/data/r/simple.R,r,26,15,26,16,132,2,5,0,0,),4,0,0
142,braced_expression,,test/data/r/simple.R,r,26,17,32,2,132,2,6,3,58,"{
    if (i %% 2 == 0) {
        print(paste(""Even:"", i))
    } else {
        print(paste(""Odd:"", i))
    }
}",176,0,3
143,{,,test/data/r/simple.R,r,26,17,26,18,142,3,0,0,0,{,4,0,0
144,if_statement,,test/data/r/simple.R,r,27,5,31,6,142,3,1,7,55,"if (i %% 2 == 0) {
        print(paste(""Even:"", i))
    } else {
        print(paste(""Odd:"", i))
    }",144,0,5
145,if,,test/data/r/simple.R,r,27,5,27,7,144,4,0,0,0,if,144,1,0
146,(,,test/data/r/simple.R,r,27,8,27,9,144,4,1,0,0,(,4,0,0
147,binary_operator,,test/data/r/simple.R,r,27,9,27,20,144,4,2,3,6,i %% 2 == 0,192,0,3
148,binary_operator,,test/data/r/simple.R,r,27,9,27,15,147,5,0,3,3,i %% 2,192,0,3
149,identifier,i,test/data/r/simple.R,r,27,9,27,10,148,6,0,0,0,i,80,0,0
150,special,%%,test/data/r/simple.R,r,27,11,27,13,148,6,1,0,0,%%,208,0,0
151,float,2,test/data/r/simple.R,r,27,14,27,15,148,6,2,0,0,2,65,0,0
152,==,,test/data/r/simple.R,r,27,16,27,18,147,5,1,0,0,==,200,0,0
153,float,0,test/data/r/simple.R,r,27,19,27,20,147,5,2,0,0,0,65,0,0
154,),,test/data/r/simple.R,r,27,20,27,21,144,4,3,0,0,),4,0,0
155,braced_expression,,test/data/r/simple.R,r,27,22,29,6,144,4,4,3,21,"{
        print(paste(""Even:"", i))
    }",176,0,3
156,{,,test/data/r/simple.R,r,27,22,27,23,155,5,0,0,0,{,4,0,0
157,call,print,test/data/r/simple.R,r,28,9,28,33,155,5,1,2,18,"print(paste(""Even:"", i))",208,0,2
158,identifier,print,test/data/r/simple.R,r,28,9,28,14,157,6,0,0,0,print,80,0,0
159,arguments,,test/data/r/simple.R,r,28,14,28,33,157,6,1,3,16,"(paste(""Even:"", i))",181,0,3
160,(,,test/data/r/simple.R,r,28,14,28,15,159,7,0,0,0,(,4,0,0
161,argument,,test/data/r/simple.R,r,28,15,28,32,159,7,1,1,13,"paste(""Even:"", i)",180,0,1
162,call,paste,test/data/r/simple.R,r,28,15,28,32,161,8,0,2,12,"paste(""Even:"", i)",208,0,2
163,identifier,paste,test/data/r/simple.R,r,28,15,28,20,162,9,0,0,0,paste,80,0,0
164,arguments,,test/data/r/simple.R,r,28,20,28,32,162,9,1,5,10,"(""Even:"", i)",181,0,4
165,(,,test/data/r/simple.R,r,28,20,28,21,164,10,0,0,0,(,4,0,0
166,argument,,test/data/r/simple.R,r,28,21,28,28,164,10,1,1,4,"""Even:""",180,0,1
167,string,"""Even:""",test/data/r/simple.R,r,28,21,28,28,166,11,0,3,3,"""Even:""",68,0,3
168,"""",,test/data/r/simple.R,r,28,21,28,22,167,12,0,0,0,"""",8,0,0
169,string_content,,test/data/r/simple.R,r,28,22,28,27,167,12,1,0,0,Even:,68,0,0
170,"""",,test/data/r/simple.R,r,28,27,28,28,167,12,2,0,0,"""",8,0,0
171,comma,,test/data/r/simple.R,r,28,28,28,29,164,10,2,0,0,",",8,0,0
172,argument,i,test/data/r/simple.R,r,28,30,28,31,164,10,3,1,1,i,180,0,1
173,identifier,i,test/data/r/simple.R,r,28,30,28,31,172,11,0,0,0,i,80,0,0
174,),,test/data/r/simple.R,r,28,31,28,32,164,10,4,0,0,),4,0,0
175,),,test/data/r/simple.R,r,28,32,28,33,159,7,2,0,0,),4,0,0
176,},,test/data/r/simple.R,r,29,5,29,6,155,5,2,0,0,},4,0,0
177,else,,test/data/r/simple.R,r,29,7,29,11,144,4,5,0,0,else,144,1,0
178,braced_expression,,test/data/r/simple.R,r,29,12,31,6,144,4,6,3,21,"{
        print(paste(""Odd:"", i))
    }",176,0,3
179,{,,test/data/r/simple.R,r,29,12,29,13,178,5,0,0,0,{,4,0,0
180,call,print,test/data/r/simple.R,r,30,9,30,32,178,5,1,2,18,"print(paste(""Odd:"", i))",208,0,2
181,identifier,print,test/data/r/simple.R,r,30,9,30,14,180,6,0,0,0,print,80,0,0
182,arguments,,test/data/r/simple.R,r,30,14,30,32,180,6,1,3,16,"(paste(""Odd:"", i))",181,0,3
183,(,,test/data/r/simple.R,r,30,14,30,15,182,7,0,0,0,(,4,0,0
184,argument,,test/data/r/simple.R,r,30,15,30,31,182,7,1,1,13,"paste(""Odd:"", i)",180,0,1
185,call,paste,test/data/r/simple.R,r,30,15,30,31,184,8,0,2,12,"paste(""Odd:"", i)",208,0,2
186,identifier,paste,test/data/r/simple.R,r,30,15,30,20,185,9,0,0,0,paste,80,0,0
187,arguments,,test/data/r/simple.R,r,30,20,30,31,185,9,1,5,10,"(""Odd:"", i)",181,0,4
188,(,,test/data/r/simple.R,r,30,20,30,21,187,10,0,0,0,(,4,0,0
189,argument,,test/data/r/simple.R,r,30,21,30,27,187,10,1,1,4,"""Odd:""",180,0,1
190,string,"""Odd:""",test/data/r/simple.R,r,30,21,30,27,189,11,0,3,3,"""Odd:""",68,0,3
191,"""",,test/data/r/simple.R,r,30,21,30,22,190,12,0,0,0,"""",8,0,0
192,string_content,,test/data/r/simple.R,r,30,22,30,26,190,12,1,0,0,Odd:,68,0,0
193,"""",,test/data/r/simple.R,r,30,26,30,27,190,12,2,0,0,"""",8,0,0
194,comma,,test/data/r/simple.R,r,30,27,30,28,187,10,2,0,0,",",8,0,0
195,argument,i,test/data/r/simple.R,r,30,29,30,30,187,10,3,1,1,i,180,0,1
196,identifier,i,test/data/r/simple.R,r,30,29,30,30,195,11,0,0,0,i,80,0,0
197,),,test/data/r/simple.R,r,30,30,30,31,187,10,4,0,0,),4,0,0
198,),,test/data/r/simple.R,r,30,31,30,32,182,7,2,0,0,),4,0,0
199,},,test/data/r/simple.R,r,31,5,31,6,178,5,2,0,0,},4,0,0
200,},,test/data/r/simple.R,r,32,1,32,2,142,3,2,0,0,},4,0,0
201,comment,,test/data/r/simple.R,r,34,1,34,13,0,1,13,0,0,"# While loop",32,0,0
202,binary_operator,,test/data/r/simple.R,r,35,1,35,13,0,1,14,3,3,counter <- 1,192,0,3
203,identifier,counter,test/data/r/simple.R,r,35,1,35,8,202,2,0,0,0,counter,80,0,0
204,<-,,test/data/r/simple.R,r,35,9,35,11,202,2,1,0,0,<-,204,0,0
205,float,1,test/data/r/simple.R,r,35,12,35,13,202,2,2,0,0,1,65,0,0
206,while_statement,,test/data/r/simple.R,r,36,1,39,2,0,1,15,5,24,"while (counter <= 5) {
    print(counter)
    counter <- counter + 1
}",150,0,4
207,while,,test/data/r/simple.R,r,36,1,36,6,206,2,0,0,0,while,148,1,0
208,(,,test/data/r/simple.R,r,36,7,36,8,206,2,1,0,0,(,4,0,0
209,binary_operator,,test/data/r/simple.R,r,36,8,36,20,206,2,2,3,3,counter <= 5,192,0,3
210,identifier,counter,test/data/r/simple.R,r,36,8,36,15,209,3,0,0,0,counter,80,0,0
211,<=,,test/data/r/simple.R,r,36,16,36,18,209,3,1,0,0,<=,200,0,0
212,float,5,test/data/r/simple.R,r,36,19,36,20,209,3,2,0,0,5,65,0,0
213,),,test/data/r/simple.R,r,36,20,36,21,206,2,3,0,0,),4,0,0
214,braced_expression,,test/data/r/simple.R,r,36,22,39,2,206,2,4,4,16,"{
    print(counter)
    counter <- counter + 1
}",176,0,4
215,{,,test/data/r/simple.R,r,36,22,36,23,214,3,0,0,0,{,4,0,0
216,call,print,test/data/r/simple.R,r,37,5,37,19,214,3,1,2,6,print(counter),208,0,2
217,identifier,print,test/data/r/simple.R,r,37,5,37,10,216,4,0,0,0,print,80,0,0
218,arguments,,test/data/r/simple.R,r,37,10,37,19,216,4,1,3,4,(counter),181,0,3
219,(,,test/data/r/simple.R,r,37,10,37,11,218,5,0,0,0,(,4,0,0
220,argument,counter,test/data/r/simple.R,r,37,11,37,18,218,5,1,1,1,counter,180,0,1
221,identifier,counter,test/data/r/simple.R,r,37,11,37,18,220,6,0,0,0,counter,80,0,0
222,),,test/data/r/simple.R,r,37,18,37,19,218,5,2,0,0,),4,0,0
223,binary_operator,,test/data/r/simple.R,r,38,5,38,27,214,3,2,3,6,counter <- counter + 1,192,0,3
224,identifier,counter,test/data/r/simple.R,r,38,5,38,12,223,4,0,0,0,counter,80,0,0
225,<-,,test/data/r/simple.R,r,38,13,38,15,223,4,1,0,0,<-,204,0,0
226,binary_operator,,test/data/r/simple.R,r,38,16,38,27,223,4,2,3,3,counter + 1,192,0,3
227,identifier,counter,test/data/r/simple.R,r,38,16,38,23,226,5,0,0,0,counter,80,0,0
228,+,,test/data/r/simple.R,r,38,24,38,25,226,5,1,0,0,+,192,0,0
229,float,1,test/data/r/simple.R,r,38,26,38,27,226,5,2,0,0,1,65,0,0
230,},,test/data/r/simple.R,r,39,1,39,2,214,3,3,0,0,},4,0,0
231,comment,,test/data/r/simple.R,r,41,1,41,17,0,1,16,0,0,"# Function calls",32,0,0
232,binary_operator,,test/data/r/simple.R,r,42,1,42,36,0,1,17,3,9,result <- calculate_mean(my_vector),192,0,3
233,identifier,result,test/data/r/simple.R,r,42,1,42,7,232,2,0,0,0,result,80,0,0
234,<-,,test/data/r/simple.R,r,42,8,42,10,232,2,1,0,0,<-,204,0,0
235,call,calculate_mean,test/data/r/simple.R,r,42,11,42,36,232,2,2,2,6,calculate_mean(my_vector),208,0,2
236,identifier,calculate_mean,test/data/r/simple.R,r,42,11,42,25,235,3,0,0,0,calculate_mean,80,0,0
237,arguments,,test/data/r/simple.R,r,42,25,42,36,235,3,1,3,4,(my_vector),181,0,3
238,(,,test/data/r/simple.R,r,42,25,42,26,237,4,0,0,0,(,4,0,0
239,argument,my_vector,test/data/r/simple.R,r,42,26,42,35,237,4,1,1,1,my_vector,180,0,1
240,identifier,my_vector,test/data/r/simple.R,r,42,26,42,35,239,5,0,0,0,my_vector,80,0,0
241,),,test/data/r/simple.R,r,42,35,42,36,237,4,2,0,0,),4,0,0
242,call,summary,test/data/r/simple.R,r,43,1,43,17,0,1,18,2,6,summary(my_list),208,0,2
243,identifier,summary,test/data/r/simple.R,r,43,1,43,8,242,2,0,0,0,summary,80,0,0
244,arguments,,test/data/r/simple.R,r,43,8,43,17,242,2,1,3,4,(my_list),181,0,3
245,(,,test/data/r/simple.R,r,43,8,43,9,244,3,0,0,0,(,4,0,0
246,argument,my_list,test/data/r/simple.R,r,43,9,43,16,244,3,1,1,1,my_list,180,0,1
247,identifier,my_list,test/data/r/simple.R,r,43,9,43,16,246,4,0,0,0,my_list,80,0,0
248,),,test/data/r/simple.R,r,43,16,43,17,244,3,2,0,0,),4,0,0
249,comment,,test/data/r/simple.R,r,45,1,45,20,0,1,19,0,0,"# Data manipulation",32,0,0
250,binary_operator,,test/data/r/simple.R,r,46,1,46,42,0,1,20,3,15,"filtered_data <- subset(mtcars, mpg > 20)",192,0,3
251,identifier,filtered_data,test/data/r/simple.R,r,46,1,46,14,250,2,0,0,0,filtered_data,80,0,0
252,<-,,test/data/r/simple.R,r,46,15,46,17,250,2,1,0,0,<-,204,0,0
253,call,subset,test/data/r/simple.R,r,46,18,46,42,250,2,2,2,12,"subset(mtcars, mpg > 20)",208,0,2
254,identifier,subset,test/data/r/simple.R,r,46,18,46,24,253,3,0,0,0,subset,80,0,0
255,arguments,,test/data/r/simple.R,r,46,24,46,42,253,3,1,5,10,"(mtcars, mpg > 20)",181,0,4
256,(,,test/data/r/simple.R,r,46,24,46,25,255,4,0,0,0,(,4,0,0
257,argument,mtcars,test/data/r/simple.R,r,46,25,46,31,255,4,1,1,1,mtcars,180,0,1
258,identifier,mtcars,test/data/r/simple.R,r,46,25,46,31,257,5,0,0,0,mtcars,80,0,0
259,comma,,test/data/r/simple.R,r,46,31,46,32,255,4,2,0,0,",",8,0,0
260,argument,,test/data/r/simple.R,r,46,33,46,41,255,4,3,1,4,mpg > 20,180,0,1
261,binary_operator,,test/data/r/simple.R,r,46,33,46,41,260,5,0,3,3,mpg > 20,192,0,3
262,identifier,mpg,test/data/r/simple.R,r,46,33,46,36,261,6,0,0,0,mpg,80,0,0
263,>,,test/data/r/simple.R,r,46,37,46,38,261,6,1,0,0,>,200,0,0
264,float,20,test/data/r/simple.R,r,46,39,46,41,261,6,2,0,0,20,65,0,0
265,),,test/data/r/simple.R,r,46,41,46,42,255,4,4,0,0,),4,0,0
266,binary_operator,,test/data/r/simple.R,r,47,1,47,62,0,1,21,3,22,"aggregated <- aggregate(mpg ~ cyl, data = mtcars, FUN = mean)",192,0,3
267,identifier,aggregated,test/data/r/simple.R,r,47,1,47,11,266,2,0,0,0,aggregated,80,0,0
268,<-,,test/data/r/simple.R,r,47,12,47,14,266,2,1,0,0,<-,204,0,0
269,call,aggregate,test/data/r/simple.R,r,47,15,47,62,266,2,2,2,19,"aggregate(mpg ~ cyl, data = mtcars, FUN = mean)",208,0,2
270,identifier,aggregate,test/data/r/simple.R,r,47,15,47,24,269,3,0,0,0,aggregate,80,0,0
271,arguments,,test/data/r/simple.R,r,47,24,47,62,269,3,1,7,17,"(mpg ~ cyl, data = mtcars, FUN = mean)",181,0,5
272,(,,test/data/r/simple.R,r,47,24,47,25,271,4,0,0,0,(,4,0,0
273,argument,,test/data/r/simple.R,r,47,25,47,34,271,4,1,1,4,mpg ~ cyl,180,0,1
274,binary_operator,,test/data/r/simple.R,r,47,25,47,34,273,5,0,3,3,mpg ~ cyl,192,0,3
275,identifier,mpg,test/data/r/simple.R,r,47,25,47,28,274,6,0,0,0,mpg,80,0,0
276,~,,test/data/r/simple.R,r,47,29,47,30,274,6,1,0,0,~,192,0,0
277,identifier,cyl,test/data/r/simple.R,r,47,31,47,34,274,6,2,0,0,cyl,80,0,0
278,comma,,test/data/r/simple.R,r,47,34,47,35,271,4,2,0,0,",",8,0,0
279,argument,data,test/data/r/simple.R,r,47,36,47,49,271,4,3,3,3,data = mtcars,180,0,3
280,identifier,data,test/data/r/simple.R,r,47,36,47,40,279,5,0,0,0,data,80,0,0
281,=,,test/data/r/simple.R,r,47,41,47,42,279,5,1,0,0,=,204,0,0
282,identifier,mtcars,test/data/r/simple.R,r,47,43,47,49,279,5,2,0,0,mtcars,80,0,0
283,comma,,test/data/r/simple.R,r,47,49,47,50,271,4,4,0,0,",",8,0,0
284,argument,FUN,test/data/r/simple.R,r,47,51,47,61,271,4,5,3,3,FUN = mean,180,0,3
285,identifier,FUN,test/data/r/simple.R,r,47,51,47,54,284,5,0,0,0,FUN,80,0,0
286,=,,test/data/r/simple.R,r,47,55,47,56,284,5,1,0,0,=,204,0,0
287,identifier,mean,test/data/r/simple.R,r,47,57,47,61,284,5,2,0,0,mean,80,0,0
288,),,test/data/r/simple.R,r,47,61,47,62,271,4,6,0,0,),4,0,0
289,comment,,test/data/r/simple.R,r,49,1,49,23,0,1,22,0,0,"# R-specific operators",32,0,0
290,binary_operator,,test/data/r/simple.R,r,50,1,52,47,0,1,23,3,37,"piped_result <- mtcars |> 
    subset(mpg > 20) |>
    aggregate(mpg ~ cyl, data = ., FUN = mean)",192,0,3
291,identifier,piped_result,test/data/r/simple.R,r,50,1,50,13,290,2,0,0,0,piped_result,80,0,0
292,<-,,test/data/r/simple.R,r,50,14,50,16,290,2,1,0,0,<-,204,0,0
293,binary_operator,,test/data/r/simple.R,r,50,17,52,47,290,2,2,3,34,"mtcars |> 
    subset(mpg > 20) |>
    aggregate(mpg ~ cyl, data = ., FUN = mean)",192,0,3
294,binary_operator,,test/data/r/simple.R,r,50,17,51,21,293,3,0,3,12,"mtcars |> 
    subset(mpg > 20)",192,0,3
295,identifier,mtcars,test/data/r/simple.R,r,50,17,50,23,294,4,0,0,0,mtcars,80,0,0
296,|>,,test/data/r/simple.R,r,50,24,50,26,294,4,1,0,0,|>,192,0,0
297,call,subset,test/data/r/simple.R,r,51,5,51,21,294,4,2,2,9,subset(mpg > 20),208,0,2
298,identifier,subset,test/data/r/simple.R,r,51,5,51,11,297,5,0,0,0,subset,80,0,0
299,arguments,,test/data/r/simple.R,r,51,11,51,21,297,5,1,3,7,(mpg > 20),181,0,3
300,(,,test/data/r/simple.R,r,51,11,51,12,299,6,0,0,0,(,4,0,0
301,argument,,test/data/r/simple.R,r,51,12,51,20,299,6,1,1,4,mpg > 20,180,0,1
302,binary_operator,,test/data/r/simple.R,r,51,12,51,20,301,7,0,3,3,mpg > 20,192,0,3
303,identifier,mpg,test/data/r/simple.R,r,51,12,51,15,302,8,0,0,0,mpg,80,0,0
304,>,,test/data/r/simple.R,r,51,16,51,17,302,8,1,0,0,>,200,0,0
305,float,20,test/data/r/simple.R,r,51,18,51,20,302,8,2,0,0,20,65,0,0
306,),,test/data/r/simple.R,r,51,20,51,21,299,6,2,0,0,),4,0,0
307,|>,,test/data/r/simple.R,r,51,22,51,24,293,3,1,0,0,|>,192,0,0
308,call,aggregate,test/data/r/simple.R,r,52,5,52,47,293,3,2,2,19,"aggregate(mpg ~ cyl, data = ., FUN = mean)",208,0,2
309,identifier,aggregate,test/data/r/simple.R,r,52,5,52,14,308,4,0,0,0,aggregate,80,0,0
310,arguments,,test/data/r/simple.R,r,52,14,52,47,308,4,1,7,17,"(mpg ~ cyl, data = ., FUN = mean)",181,0,5
311,(,,test/data/r/simple.R,r,52,14,52,15,310,5,0,0,0,(,4,0,0
312,argument,,test/data/r/simple.R,r,52,15,52,24,310,5,1,1,4,mpg ~ cyl,180,0,1
313,binary_operator,,test/data/r/simple.R,r,52,15,52,24,312,6,0,3,3,mpg ~ cyl,192,0,3
314,identifier,mpg,test/data/r/simple.R,r,52,15,52,18,313,7,0,0,0,mpg,80,0,0
315,~,,test/data/r/simple.R,r,52,19,52,20,313,7,1,0,0,~,192,0,0
316,identifier,cyl,test/data/r/simple.R,r,52,21,52,24,313,7,2,0,0,cyl,80,0,0
317,comma,,test/data/r/simple.R,r,52,24,52,25,310,5,2,0,0,",",8,0,0
318,argument,data,test/data/r/simple.R,r,52,26,52,34,310,5,3,3,3,data = .,180,0,3
319,identifier,data,test/data/r/simple.R,r,52,26,52,30,318,6,0,0,0,data,80,0,0
320,=,,test/data/r/simple.R,r,52,31,52,32,318,6,1,0,0,=,204,0,0
321,identifier,.,test/data/r/simple.R,r,52,33,52,34,318,6,2,0,0,.,80,0,0
322,comma,,test/data/r/simple.R,r,52,34,52,35,310,5,4,0,0,",",8,0,0
323,argument,FUN,test/data/r/simple.R,r,52,36,52,46,310,5,5,3,3,FUN = mean,180,0,3
324,identifier,FUN,test/data/r/simple.R,r,52,36,52,39,323,6,0,0,0,FUN,80,0,0
325,=,,test/data/r/simple.R,r,52,40,52,41,323,6,1,0,0,=,204,0,0
326,identifier,mean,test/data/r/simple.R,r,52,42,52,46,323,6,2,0,0,mean,80,0,0
327,),,test/data/r/simple.R,r,52,46,52,47,310,5,6,0,0,),4,0,0
328,comment,,test/data/r/simple.R,r,54,1,54,17,0,1,24,0,0,"# Special values",32,0,0
329,binary_operator,,test/data/r/simple.R,r,55,1,55,20,0,1,25,3,4,missing_value <- NA,192,0,3
330,identifier,missing_value,test/data/r/simple.R,r,55,1,55,14,329,2,0,0,0,missing_value,80,0,0
331,<-,,test/data/r/simple.R,r,55,15,55,17,329,2,1,0,0,<-,204,0,0
332,na,NA,test/data/r/simple.R,r,55,18,55,20,329,2,2,1,1,NA,72,0,1
333,NA,NA,test/data/r/simple.R,r,55,18,55,20,332,3,0,0,0,NA,72,0,0
334,binary_operator,,test/data/r/simple.R,r,56,1,56,22,0,1,26,3,3,infinite_value <- Inf,192,0,3
335,identifier,infinite_value,test/data/r/simple.R,r,56,1,56,15,334,2,0,0,0,infinite_value,80,0,0
336,<-,,test/data/r/simple.R,r,56,16,56,18,334,2,1,0,0,<-,204,0,0
337,inf,Inf,test/data/r/simple.R,r,56,19,56,22,334,2,2,0,0,Inf,72,0,0
338,binary_operator,,test/data/r/simple.R,r,57,1,57,25,0,1,27,3,7,complex_number <- 1 + 2i,192,0,3
339,identifier,complex_number,test/data/r/simple.R,r,57,1,57,15,338,2,0,0,0,complex_number,80,0,0
340,<-,,test/data/r/simple.R,r,57,16,57,18,338,2,1,0,0,<-,204,0,0
341,binary_operator,,test/data/r/simple.R,r,57,19,57,25,338,2,2,3,4,1 + 2i,192,0,3
342,float,1,test/data/r/simple.R,r,57,19,57,20,341,3,0,0,0,1,65,0,0
343,+,,test/data/r/simple.R,r,57,21,57,22,341,3,1,0,0,+,192,0,0
344,complex,2i,test/data/r/simple.R,r,57,23,57,25,341,3,2,1,1,2i,67,0,1
345,i,i,test/data/r/simple.R,r,57,24,57,25,344,4,0,0,0,i,112,0,0
346,comment,,test/data/r/simple.R,r,59,1,59,29,0,1,28,0,0,"# Comments and documentation",32,0,0
347,comment,,test/data/r/simple.R,r,60,1,60,32,0,1,29,0,0,"# This is a single line comment",32,0,0
348,comment,,test/data/r/simple.R,r,62,1,62,37,0,1,30,0,0,"# Private function (starts with dot)",32,0,0
349,binary_operator,,test/data/r/simple.R,r,63,1,65,2,0,1,31,3,22,".private_helper <- function(x) {
    return(x * 2)
}",192,0,3
350,identifier,.private_helper,test/data/r/simple.R,r,63,1,63,16,349,2,0,0,0,.private_helper,80,0,0
351,<-,,test/data/r/simple.R,r,63,17,63,19,349,2,1,0,0,<-,204,0,0
352,function_definition,.private_helper,test/data/r/simple.R,r,63,20,65,2,349,2,2,3,19,"function(x) {
    return(x * 2)
}",240,0,3
353,function,,test/data/r/simple.R,r,63,20,63,28,352,3,0,0,0,function,240,1,0
354,parameters,,test/data/r/simple.R,r,63,28,63,31,352,3,1,3,4,(x),181,0,3
355,(,,test/data/r/simple.R,r,63,28,63,29,354,4,0,0,0,(,4,0,0
356,parameter,x,test/data/r/simple.R,r,63,29,63,30,354,4,1,1,1,x,246,0,1
357,identifier,x,test/data/r/simple.R,r,63,29,63,30,356,5,0,0,0,x,80,0,0
358,),,test/data/r/simple.R,r,63,30,63,31,354,4,2,0,0,),4,0,0
359,braced_expression,,test/data/r/simple.R,r,63,32,65,2,352,3,2,3,12,"{
    return(x * 2)
}",176,0,3
360,{,,test/data/r/simple.R,r,63,32,63,33,359,4,0,0,0,{,4,0,0
361,call,,test/data/r/simple.R,r,64,5,64,18,359,4,1,2,9,return(x * 2),208,0,2
362,return,,test/data/r/simple.R,r,64,5,64,11,361,5,0,0,0,return,152,1,0
363,arguments,,test/data/r/simple.R,r,64,11,64,18,361,5,1,3,7,(x * 2),181,0,3
364,(,,test/data/r/simple.R,r,64,11,64,12,363,6,0,0,0,(,4,0,0
365,argument,,test/data/r/simple.R,r,64,12,64,17,363,6,1,1,4,x * 2,180,0,1
366,binary_operator,,test/data/r/simple.R,r,64,12,64,17,365,7,0,3,3,x * 2,192,0,3
367,identifier,x,test/data/r/simple.R,r,64,12,64,13,366,8,0,0,0,x,80,0,0
368,*,,test/data/r/simple.R,r,64,14,64,15,366,8,1,0,0,*,192,0,0
369,float,2,test/data/r/simple.R,r,64,16,64,17,366,8,2,0,0,2,65,0,0
370,),,test/data/r/simple.R,r,64,17,64,18,363,6,2,0,0,),4,0,0
371,},,test/data/r/simple.R,r,65,1,65,2,359,4,2,0,0,},4,0,0
372,comment,,test/data/r/simple.R,r,67,1,67,21,0,1,32,0,0,"# Package operations",32,0,0
373,call,library,test/data/r/simple.R,r,68,1,68,15,0,1,33,2,6,library(dplyr),208,0,2
374,identifier,library,test/data/r/simple.R,r,68,1,68,8,373,2,0,0,0,library,80,0,0
375,arguments,,test/data/r/simple.R,r,68,8,68,15,373,2,1,3,4,(dplyr),181,0,3
376,(,,test/data/r/simple.R,r,68,8,68,9,375,3,0,0,0,(,4,0,0
377,argument,dplyr,test/data/r/simple.R,r,68,9,68,14,375,3,1,1,1,dplyr,180,0,1
378,identifier,dplyr,test/data/r/simple.R,r,68,9,68,14,377,4,0,0,0,dplyr,80,0,0
379,),,test/data/r/simple.R,r,68,14,68,15,375,3,2,0,0,),4,0,0
380,comment,,test/data/r/simple.R,r,69,1,69,32,0,1,34,0,0,"# import::from(magrittr, ""%>%"")",32,0,0
