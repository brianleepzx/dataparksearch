SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
SQL>'SELECT status, docsize, hops, crc32, pop_rank, url FROM url ORDER BY status, crc32'
200	807	1	-1737037344	0.25	http://site/testpage12.html
200	986	1	-1118632054	0.25	http://site/testpage1.html
200	1729	1	-558329651	0.25	http://site/testpage9.html
200	987	1	-430346198	0.25	http://site/testpage3.html
200	894	1	-300395911	0.25	http://site/testpage13.html
200	398	0	0	0.25	http://site/
200	1041	1	182328450	0.25	http://site/testpage11.html
200	1023	1	599907317	0.25	http://site/testpage10.html
200	828	1	901144470	0.25	http://site/testpage5.html
200	992	1	1149098368	0.25	http://site/testpage2.html
200	924	1	1416251075	0.25	http://site/testpage4.html
200	2989	1	1761578894	0.25	http://site/testpage8.html
200	813	1	1794430673	0.25	http://site/testpage7.html
200	941	1	2030409970	0.25	http://site/testpage6.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,lower(urlinfo.sname)'
200	-1737037344	http://site/testpage12.html	body	The Wall Street Journal Sun, 30 Apr 2006 23:06:00 EDT rewrite
200	-1737037344	http://site/testpage12.html	Content-Type	text/html
200	-1737037344	http://site/testpage12.html	title	TestPage12 Within Three Months
200	-1118632054	http://site/testpage1.html	body	The Wall Street Journal Thu, 25 May 2006 23:06:00 EDT BODY1 BODY2 BODY3 BODY4 BODY5 BODY6 BODY7 BODY8 BODY9 BODY10 BODY11 BODY12 BODY13 BODY14 BODY15 BODY16 BODY17 BODY18 BODY19 BODY20 BODY21 BODY22 BODY23 BODY24 BODY25 BODY26 BODY27 BODY28 BODY29 BODY30
200	-1118632054	http://site/testpage1.html	Content-Type	text/html
200	-1118632054	http://site/testpage1.html	title	TestPage1
200	-558329651	http://site/testpage9.html	body	The Wall Street Journal The Wall Street Journal The Wall Street Journal Mon, 24 May 2005 23:06:00 EDT On the walls of the city. Shadowed streets. Unknown journals. rewrite
200	-558329651	http://site/testpage9.html	Content-Type	text/html
200	-558329651	http://site/testpage9.html	title	TestPage9 title1 foo2 foo3 foo4 foo5 foo6 foo7 foo8 foo9 foo10 foo11 foo12 foo13 foo14 foo15 foo16 foo17 foo18 foo19 foo20 title
200	-430346198	http://site/testpage3.html	body	The Wall Street Journal Thu, 26 May 2006 23:06:00 EDT BODY2 BODY4 BODY6 BODY8 BODY10 BODY12 BODY14 BODY16 BODY18 BODY20 BODY22 BODY24 BODY26 BODY28 BODY30 BODY31 BODY32 BODY33 BODY34 BODY35 BODY36 BODY37 BODY38 BODY39 BODY40
200	-430346198	http://site/testpage3.html	Content-Type	text/html
200	-430346198	http://site/testpage3.html	title	TestPage3 Even Number of BODY's until 30
200	-300395911	http://site/testpage13.html	body	The Wall Street Journal Tue, 31 Jan 2006 23:06:00 EDT rewrite This is a real-time news story and may be updated in the near future.
200	-300395911	http://site/testpage13.html	Content-Type	text/html
200	-300395911	http://site/testpage13.html	title	TestPage13 Within 6 months
200	0	http://site/	Content-type	text/html
200	182328450	http://site/testpage11.html	body	The Wall Street Journal Mon, 24 May 2005 23:06:00 EDT amit amit amit aaaaaaaaaabbbbbbbbbbcccccccccc aaaaaaaaaabbbbbbbbbbccccccccccdd aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeee aaaaaaaaaabbbbbbbbbbccccccccccd
200	182328450	http://site/testpage11.html	Content-Type	text/html
200	182328450	http://site/testpage11.html	title	TestPage11 Long Words
200	599907317	http://site/testpage10.html	body	The Wall Street Journal Mon, 24 May 2005 23:06:00 EDT aaaaaaaaaabbbbbbbbbbcccccccccc aaaaaaaaaabbbbbbbbbbccccccccccdd aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeee aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeee
200	599907317	http://site/testpage10.html	Content-Type	text/html
200	599907317	http://site/testpage10.html	title	TestPage10 Long Words
200	901144470	http://site/testpage5.html	body	The Wall Street Journal Mon, 24 May 2006 23:06:00 EDT American typo Insurance Group
200	901144470	http://site/testpage5.html	Content-Type	text/html
200	901144470	http://site/testpage5.html	title	TestPage5 Phrase searching
200	1149098368	http://site/testpage2.html	body	The Wall Street Journal Thu, 25 May 2006 23:06:00 EDT BODY1 BODY3 BODY5 BODY7 BODY9 BODY11 BODY13 BODY15 BODY17 BODY19 BODY21 BODY23 BODY25 BODY27 BODY29 BODY30 BODY31 BODY32 BODY33 BODY34 BODY35 BODY36 BODY37 BODY38 BODY39 BODY40
200	1149098368	http://site/testpage2.html	Content-Type	text/html
200	1149098368	http://site/testpage2.html	title	TestPage2 Odd Number of BODY's until 30
200	1416251075	http://site/testpage4.html	body	The Wall Street Journal Mon, 25 May 2006 23:06:00 EDT The government has a million dollar system. The policy comes from the American Insurance Group, or AIG.
200	1416251075	http://site/testpage4.html	Content-Type	text/html
200	1416251075	http://site/testpage4.html	title	Testpage4 Stopwords and Phrase Search Check
200	1761578894	http://site/testpage8.html	body	The Wall Street Journal Wed, 24 May 2002 23:06:00 EDT word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12 word13 word14 word15 word16 word17 word18 word19 word20 word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 wor
200	1761578894	http://site/testpage8.html	Content-Type	text/html
200	1761578894	http://site/testpage8.html	title	TestPage8 300 Word Body
200	1794430673	http://site/testpage7.html	body	The Wall Street Journal Wed, 24 May 2006 23:06:00 EDT title1
200	1794430673	http://site/testpage7.html	Content-Type	text/html
200	1794430673	http://site/testpage7.html	title	TestPage7 Search Body Content only
200	2030409970	http://site/testpage6.html	body	The Wall Street Journal Wed, 24 May 2006 23:06:00 EDT machine1
200	2030409970	http://site/testpage6.html	Content-Type	text/html
200	2030409970	http://site/testpage6.html	title	TestPage6 title1 title2 title3 title4 title5 title6 title7 title8 title9 title10 title11 title12 title13 title14 title15 title16
SQL>'SELECT u1.docsize,u2.docsize,u1.url,u2.url FROM url u1,url u2, links l WHERE u1.rec_id=l.ot AND u2.rec_id=l.k ORDER BY u1.docsize,u2.docsize'
398	398	http://site/	http://site/
398	807	http://site/	http://site/testpage12.html
398	813	http://site/	http://site/testpage7.html
398	828	http://site/	http://site/testpage5.html
398	894	http://site/	http://site/testpage13.html
398	924	http://site/	http://site/testpage4.html
398	941	http://site/	http://site/testpage6.html
398	986	http://site/	http://site/testpage1.html
398	987	http://site/	http://site/testpage3.html
398	992	http://site/	http://site/testpage2.html
398	1023	http://site/	http://site/testpage10.html
398	1041	http://site/	http://site/testpage11.html
398	1729	http://site/	http://site/testpage9.html
398	2989	http://site/	http://site/testpage8.html
807	807	http://site/testpage12.html	http://site/testpage12.html
813	813	http://site/testpage7.html	http://site/testpage7.html
828	828	http://site/testpage5.html	http://site/testpage5.html
894	894	http://site/testpage13.html	http://site/testpage13.html
924	924	http://site/testpage4.html	http://site/testpage4.html
941	941	http://site/testpage6.html	http://site/testpage6.html
986	986	http://site/testpage1.html	http://site/testpage1.html
987	987	http://site/testpage3.html	http://site/testpage3.html
992	992	http://site/testpage2.html	http://site/testpage2.html
1023	1023	http://site/testpage10.html	http://site/testpage10.html
1041	1041	http://site/testpage11.html	http://site/testpage11.html
1729	1729	http://site/testpage9.html	http://site/testpage9.html
2989	2989	http://site/testpage8.html	http://site/testpage8.html
SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>