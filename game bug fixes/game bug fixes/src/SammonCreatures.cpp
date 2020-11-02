/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// фикс SOD заклинания клон при использовании на двухклеточных существах
int __stdcall Y_FixClone_SOD_CreatureDoubleWide(LoHook* h, HookContext* c)
{
    int mainHexID = c->ecx;
    int aroundMainHexID = c->edx;
    _BattleStack_* stack = *(_BattleStack_**)(c->ebp +8);

    // если одноклеточное существо - выполняем оригинальный код
    if (!(stack->creature.flags & BCF_2HEX_WIDE) )
        return EXEC_DEFAULT;

    // если выход за границы - выполняем оригинальный код
    if (mainHexID < 0 || mainHexID > 187)
        return EXEC_DEFAULT;

    // если это гекс самого существа - выполняем оригинальный код
    if (aroundMainHexID >= 6)
        return EXEC_DEFAULT;

    // проверяем поворот гекса (1-атакующий, 0-защитник)
    if (stack->orientation) { 
        if (aroundMainHexID == 4) // если задний гекс нападающего
            mainHexID--; // главный гекс делаем гекс под жопой монстра
    } else { 
        if (aroundMainHexID == 1) // если задний гекс защитника
            mainHexID++; // главный гекс делаем гекс под жопой монстра
    }

    // если выход за границы - возвращаем нет найденого гекса
    if(mainHexID < 0 || mainHexID > 187)
        c->eax = -1;
    else {
        c->eax = o_BattleMgr->adjacentSquares[mainHexID].hexAdjacent[aroundMainHexID];
    }

    c->return_address = 0x5A709A;
    return NO_EXEC_DEFAULT;
} 


/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// объявляем функцию ВОГа (вызов существ), для более лёгкого её использования
_BattleStack_* WOG_SammonCreatureEx(int type, int count, int pos, int side, int slot, int redraw, int flags)
{
	return CALL_7(_BattleStack_*, __cdecl, 0x714D22, type, count, pos, side, slot, redraw, flags);
}

// функция проверки гекса на занят ли он (0-пуст, 1-занят)
char __fastcall Y_BattleMgr_IsHexNotFree(signed int gexID)
{
	// проверяем, правильно ли передали в функцию номер гекса
	if (gexID < 0 || gexID > 187)
		return 1; // не правильно, значит гекс занят

	// проверяем есть ли живой стек в этом гексе
	_BattleStack_* stack = o_BattleMgr->hex[gexID].GetCreature();
	if ( stack && stack->count_current > 0 )
		return 1; // стек с кол-вом существ больше нуля существует

	// теперь пусть работает SOD функция: есть ли на этом гексе стена или препятствие
	// она тоже возвращает, что гекс (0-пуст, 1-занят)
	return CALL_2(char, __thiscall, 0x4695F0, o_BattleMgr, gexID);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

signed int __fastcall SOD_GetSquareAroundThis(signed int centerGex, signed int adjacentGexID)
{
	// по умолчанию: гекс не найден
	int result = -1; 

	// оригинал: return CALL_2(signed int, __fastcall, 0x524370, centerGex, adjacentGexID);

	// если вышли за границы поля битвы: возвращаем "гекс не найден"
	if (centerGex < 0 || centerGex > 187)
		return result;
	
	// получаем номер соседнего гекса из массива менеджера битвы
	result = o_BattleMgr->adjacentSquares[centerGex].hexAdjacent[adjacentGexID];

	// если вышли за границы поля битвы: возвращаем "гекс не найден"
	if (result < 0 || result > 187)
		result = -1;

	// проверяем гекс на препятсвия и другие гексы
	// 0 = гекс пуст
	// 1 = гекс занят
	if ( Y_BattleMgr_IsHexNotFree(result) )
		result = -1;
	
	// возвращаем id найденного гекса
	return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// список приоритета поиска гексов вокруг главного: aroundGexes1[сторона][порядковый_номер_окружающего_гекса]
// порядковый_номер_это[пер.серед, пер.верх, пер.низ, задн.верх.вперёд, задн.вниз.вперёд, задн.верх.назад, задн.вниз.назад, задн.середина]
int aroundGexes1[2][8] = { 
	{1, 0, 2, 5, 3, 5, 3, 4}, 
	{4, 5, 3, 0, 2, 0, 2, 1} };
// список понимания какой гекс искать при поиске для двухгексового существа
// aroundGexes1[сторона][порядковый_номер_главного_искомого_гекса] (см. aroundGexes1[side][n])
// у атакующего    порядковый_номер_это[1=искать спереди, 4=искать сзади]
// у защищающегося порядковый_номер_это[4=искать спереди, 1=искать сзади]
int aroundGexes2[2][8] = { 
	{1, 1, 1, 1, 1, 4, 4, 4}, 
	{4, 4, 4, 4, 4, 1, 1, 1} };

// визуальние объяснение механики вызова клонов тут:
// http://wforum.heroes35.net/showthread.php?tid=5717&pid=118391#pid118391

// главная функция исправления бага вызываемых клонов в ВОГе
int __stdcall Y_FixClone(LoHook* h, HookContext* c)
{	
	// получаем стек (далее - главный стек), который вызывает клонов
	_BattleStack_* stack = (_BattleStack_*)(c->edx);

	// получаем параметры вызываемого стека
	int type = DwordAt(c->ebp -0x58);   
	int count = DwordAt(c->ebp -0x34);
	int side = stack->side;
	int redraw = DwordAt(c->ebp -0x68); 
	int flags = DwordAt(c->ebp -0x4C);

	// получаем позицию главного стека
	int mainPosition1 = stack->hex_ix; // первый гекс стека
	int mainPosition2 = mainPosition1; // второй гекс стека
	if ( stack->creature.flags & BCF_2HEX_WIDE )		
		mainPosition2 += stack->orientation ? 1 : -1; 
		// определяем второй гекс (этот гекс спереди существа)

	// получаем размер вызываемого существа
	int summonWides = 1;
	if ( o_CreatureInfo[type].flags & BCF_2HEX_WIDE )
		summonWides = 2; // двойной гекс у вызываемого стека

	int pos1; // это будет первый гекс вызываемого стека
	int pos2; // это будет второй гекс вызываемого стека

	for (int i = 0; i < 8; i++) {
		int around1 = aroundGexes1[side][i]; // получаем номер соседнего гекса
		int pos = mainPosition1; // заносим позицию одногексового существа

		// атакующий двухгексовый, и в цикле соседний гекс - его передний (1,0,2,...)
		if ( !side && around1 <= 2 ) // не паримся: для одногексовых mainPosition2 = mainPosition1
			pos = mainPosition2; // переводим "взгляд" поиска на передний гекс

		// защитник двухгексовый, и в цикле соседний гекс - его передний (4,5,3,...)
		if ( side && around1 >= 3 ) // не паримся: для одногексовых mainPosition2 = mainPosition1
			pos = mainPosition2;  // переводим "взгляд" поиска на передний гекс

		// ищем номер гекса, куда вызывается существо (одногексовый или двухгексовый_и_его_первый_гекс)
		pos1 = SOD_GetSquareAroundThis(pos, around1); 

		// не паримся: 
		// для двухгексовых расчет пойдет дальше... 
		// для одногексовых просто копирование... 
		// .. и потом проверка для обоих видов позиций
		pos2 = pos1; 

		// если главная позиция найдена, а стек - двухгесовый:
		if (pos1 != -1 && summonWides) {
			// ищем номер второго гекса, куда вызывается существо
			int around2 = aroundGexes2[side][i]; 

			// рассматриваем передний (для атк=1, зщт=4) или задний (для атк=4 или зщт=1) гекс
			pos2 = SOD_GetSquareAroundThis(pos1, around2);

			// для атакующего вызываемого
			// при вызове в гексах сзади (=4)
			// нужно обменять главный и второй гекс
			if (!side && around2 == 4) {
				int temp = pos2;
				pos2 = pos1;
				pos1 = temp;
			}

			// для защищающегося вызываемого
			// при вызове в гексах сзади (=1)
			// нужно обменять главный и второй гекс
			if (side && around2 == 1) {
				int temp = pos2;
				pos2 = pos1;
				pos1 = temp;
			}
		}

		// если гексы найдены - останавливаем поиск:
		// для одногексового: pos1 == pos2 
		// для двухгексового: pos1 != pos2 
		if ( pos1 != -1 && pos2 != -1)
			break;
	}

	// если позиции найдены, то вызываем монстра
	if (pos1 != -1 && pos2 != -1 ) {
		WOG_SammonCreatureEx(type, count, pos1, side, -1, redraw, flags);
	}
	
	// обходим все расчёты ВОГа
	c->return_address = 0x71E3F5;
	return NO_EXEC_DEFAULT;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void WOG_SammonCreatures(PatcherInstance* _PI)
{
    // фикс заклинания клон при использовании на двухклеточных существах SOD
    _PI->WriteLoHook(0x5A7095, Y_FixClone_SOD_CreatureDoubleWide);
	// фикс вызова клонов ВОГом от опыта монстров
	_PI->WriteLoHook(0x71E031, Y_FixClone);
}