tiles = {
   tile{x=418.94882233485,y=600,q=-4,r=0,tile="hills"},
   tile{x=466.58021954299,y=682.5,q=-4,r=1,tile="hills"},
   tile{x=514.21161675114,y=765,q=-4,r=2,tile="hills"},
   tile{x=561.84301395928,y=847.5,q=-4,r=3,tile="hills"},
   tile{x=609.47441116742,y=930,q=-4,r=4,tile="hills"},
   tile{x=466.58021954299,y=517.5,q=-3,r=-1,tile="hills"},
   tile{x=514.21161675114,y=600,q=-3,r=0,tile="hills"},
   tile{x=561.84301395928,y=682.5,q=-3,r=1,tile="hills"},
   tile{x=609.47441116742,y=765,q=-3,r=2,tile="hills"},
   tile{x=657.10580837557,y=847.5,q=-3,r=3,tile="hills"},
   tile{x=704.73720558371,y=930,q=-3,r=4,tile="hills"},
   tile{x=514.21161675114,y=435,q=-2,r=-2,tile="hills"},
   tile{x=561.84301395928,y=517.5,q=-2,r=-1,tile="hills"},
   tile{x=609.47441116742,y=600,q=-2,r=0,tile="hills"},
   tile{x=657.10580837557,y=682.5,q=-2,r=1,tile="hills"},
   tile{x=704.73720558371,y=765,q=-2,r=2,tile="hills"},
   tile{x=752.36860279186,y=847.5,q=-2,r=3,tile="hills"},
   tile{x=800,y=930,q=-2,r=4,tile="hills"},
   tile{x=561.84301395928,y=352.5,q=-1,r=-3,tile="hills"},
   tile{x=609.47441116742,y=435,q=-1,r=-2,tile="hills"},
   tile{x=657.10580837557,y=517.5,q=-1,r=-1,tile="hills"},
   tile{x=704.73720558371,y=600,q=-1,r=0,tile="hills"},
   tile{x=752.36860279186,y=682.5,q=-1,r=1,tile="hills"},
   tile{x=800,y=765,q=-1,r=2,tile="hills"},
   tile{x=847.63139720814,y=847.5,q=-1,r=3,tile="hills"},
   tile{x=895.26279441629,y=930,q=-1,r=4,tile="hills"},
   tile{x=609.47441116742,y=270,q=0,r=-4,tile="hills"},
   tile{x=657.10580837557,y=352.5,q=0,r=-3,tile="hills"},
   tile{x=704.73720558371,y=435,q=0,r=-2,tile="hills"},
   tile{x=752.36860279186,y=517.5,q=0,r=-1,tile="hills"},
   tile{x=800,y=600,q=0,r=0,tile="hills"},
   tile{x=847.63139720814,y=682.5,q=0,r=1,tile="lake"},
   tile{x=895.26279441629,y=765,q=0,r=2,tile="lake"},
   tile{x=942.89419162443,y=847.5,q=0,r=3,tile="hills"},
   tile{x=990.52558883258,y=930,q=0,r=4,tile="hills"},
   tile{x=704.73720558371,y=270,q=1,r=-4,tile="lake"},
   tile{x=752.36860279186,y=352.5,q=1,r=-3,tile="lake"},
   tile{x=800,y=435,q=1,r=-2,tile="lake"},
   tile{x=847.63139720814,y=517.5,q=1,r=-1,tile="lake"},
   tile{x=895.26279441629,y=600,q=1,r=0,tile="lake"},
   tile{x=942.89419162443,y=682.5,q=1,r=1,tile="lake"},
   tile{x=990.52558883258,y=765,q=1,r=2,tile="lake"},
   tile{x=1038.1569860407,y=847.5,q=1,r=3,tile="hills"},
   tile{x=800,y=270,q=2,r=-4,tile="hills"},
   tile{x=847.63139720814,y=352.5,q=2,r=-3,tile="hills"},
   tile{x=895.26279441629,y=435,q=2,r=-2,tile="hills"},
   tile{x=942.89419162443,y=517.5,q=2,r=-1,tile="hills"},
   tile{x=990.52558883258,y=600,q=2,r=0,tile="hills"},
   tile{x=1038.1569860407,y=682.5,q=2,r=1,tile="lake"},
   tile{x=1085.7883832489,y=765,q=2,r=2,tile="hills"},
   tile{x=895.26279441629,y=270,q=3,r=-4,tile="hills"},
   tile{x=942.89419162443,y=352.5,q=3,r=-3,tile="hills"},
   tile{x=990.52558883258,y=435,q=3,r=-2,tile="hills"},
   tile{x=1038.1569860407,y=517.5,q=3,r=-1,tile="hills"},
   tile{x=1085.7883832489,y=600,q=3,r=0,tile="hills"},
   tile{x=1133.419780457,y=682.5,q=3,r=1,tile="hills"},
   tile{x=990.52558883258,y=270,q=4,r=-4,tile="hills"},
   tile{x=1038.1569860407,y=352.5,q=4,r=-3,tile="hills"},
   tile{x=1085.7883832489,y=435,q=4,r=-2,tile="hills"},
   tile{x=1133.419780457,y=517.5,q=4,r=-1,tile="hills"},
   tile{x=1181.0511776652,y=600,q=4,r=0,tile="hills"},
}

red_meeple = meeple{x=990.52558883258,y= 600,team="red"}
blue_meeple = meeple{x=514.21161675114,y= 600,team="blue"}

manual_move(red_meeple, get_tile(2, 0))
manual_move(blue_meeple, get_tile(-3, 0))